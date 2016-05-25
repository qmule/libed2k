/*

Copyright (c) 2006, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#include "libed2k/pch.hpp"

#include <utility>
#include <boost/bind.hpp>
#include <boost/function/function1.hpp>

#include "libed2k/io.hpp"
#include "libed2k/bencode.hpp"
#include "libed2k/hasher.hpp"
#include "libed2k/alert_types.hpp"
#include "libed2k/alert.hpp"
#include "libed2k/socket.hpp"
#include "libed2k/random.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/kademlia/node_id.hpp"
#include "libed2k/kademlia/rpc_manager.hpp"
#include "libed2k/kademlia/routing_table.hpp"
#include "libed2k/kademlia/node.hpp"

#include "libed2k/kademlia/refresh.hpp"
#include "libed2k/kademlia/find_data.hpp"
#include "libed2k/rsa.hpp"

namespace libed2k { namespace dht
{
using detail::write_endpoint;

// TODO: configurable?
enum { announce_interval = 30 };

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
LIBED2K_DEFINE_LOG(node)
extern int g_announces;
extern int g_failed_announces;
#endif

// remove peers that have timed out
void purge_peers(std::set<peer_entry>& peers)
{
	for (std::set<peer_entry>::iterator i = peers.begin()
		  , end(peers.end()); i != end;)
	{
		// the peer has timed out
		if (i->added + minutes(int(announce_interval * 1.5f)) < time_now())
		{
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
			LIBED2K_LOG(node) << "peer timed out at: " << i->addr;
#endif
			peers.erase(i++);
		}
		else
			++i;
	}
}

void nop() {}

node_impl::node_impl(libed2k::alert_manager& alerts
	, bool (*f)(void*, const udp_message&, udp::endpoint const&, int)
	, dht_settings const& settings
	, node_id nid
	, address const& external_address
	, uint16_t port
	, external_ip_fun ext_ip, void* userdata)
	: m_settings(settings)
	, m_id(nid == (node_id::min)() || !verify_id(nid, external_address) ? generate_id(external_address) : nid)
	, m_table(m_id, 10, settings)
	, m_rpc(m_id, m_table, f, userdata, port)
	, m_ext_ip(ext_ip)
	, m_last_tracker_tick(time_now())
	, m_alerts(alerts)
	, m_send(f)
	, m_userdata(userdata)
    , m_port(port)
{
}

void node_impl::refresh(node_id const& id
	, find_data::nodes_callback const& f)
{
	boost::intrusive_ptr<dht::refresh> r(new dht::refresh(*this, id, f));
	r->start();
}

void node_impl::bootstrap(std::vector<udp::endpoint> const& nodes
	, find_data::nodes_callback const& f)
{
	boost::intrusive_ptr<dht::refresh> r(new dht::bootstrap(*this, m_id, f));

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
	int count = 0;
#endif

	for (std::vector<udp::endpoint>::const_iterator i = nodes.begin()
		, end(nodes.end()); i != end; ++i)
	{
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
		++count;
#endif
		r->add_entry(node_id(0), *i, observer::flag_initial);
	}
	
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
	LIBED2K_LOG(node) << "bootstrapping with " << count << " nodes";
#endif
	r->start();
}

int node_impl::bucket_size(int bucket)
{
	return m_table.bucket_size(bucket);
}

void node_impl::unreachable(udp::endpoint const& ep)
{
	m_rpc.unreachable(ep);
}

namespace
{

  void search_keywords_fun(std::vector<std::pair<node_entry, std::string> > const& v,
    node_impl& node, int listen_port, node_id const& target) {
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
    LIBED2K_LOG(node) << "sending search keywords [ target: " << target << " port: " << listen_port
      << " total nodes: " << v.size() << " ]";
#endif
    for (std::vector<std::pair<node_entry, std::string> >::const_iterator i = v.begin()
      , end(v.end()); i != end; ++i)
    {
        kad2_search_key_req req;
        req.start_position = 0;
        req.target_id = target;
        node.m_rpc.invoke(req, i->first.ep(), observer_ptr());
    }
  }

  void search_notes_fun(std::vector<std::pair<node_entry, std::string> > const& v,
    node_impl& node, int listen_port, node_id const& target) {
    // do nothing now
  }

    void search_sources_fun(std::vector<std::pair<node_entry, std::string> > const& v
    , node_impl& node
    , int listen_port
    , size_type size
    , node_id const& target) {
    #ifdef LIBED2K_DHT_VERBOSE_LOGGING
    LIBED2K_LOG(node) << "sending search sources [ target: " << target << " port: " << listen_port
        << " total nodes: " << v.size() << " ]";
    #endif
    for (std::vector<std::pair<node_entry, std::string> >::const_iterator i = v.begin()
        , end(v.end()); i != end; ++i) {
            kad2_search_sources_req req;
            req.start_position = 0;
            req.target_id = target;
            req.size = size;
            node.m_rpc.invoke(req, i->first.ep(), observer_ptr());
        }
    }

  // will be in publish 
	void announce_fun(std::vector<std::pair<node_entry, std::string> > const& v
		, node_impl& node, int listen_port, node_id const& ih)
	{
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
		LIBED2K_LOG(node) << "sending announce_peer [ ih: " << ih
			<< " p: " << listen_port
			<< " nodes: " << v.size() << " ]" ;
#endif

		// create a dummy traversal_algorithm		
		boost::intrusive_ptr<traversal_algorithm> algo(
			new traversal_algorithm(node, (node_id::min)()));

		// store on the first k nodes
		for (std::vector<std::pair<node_entry, std::string> >::const_iterator i = v.begin()
			, end(v.end()); i != end; ++i)
		{
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
			LIBED2K_LOG(node) << "  distance: " << (distance_exp(ih, i->first.id));
#endif

			void* ptr = node.m_rpc.allocate_observer();
			if (ptr == 0) return;
			observer_ptr o(new (ptr) announce_observer(algo, i->first.ep(), i->first.id));
#if defined LIBED2K_DEBUG || LIBED2K_RELEASE_ASSERTS
			o->m_in_constructor = false;
#endif
            // will be publish req packet
            kad2_search_key_req req;
            req.start_position = 0;
            req.target_id = ih;
            node.m_rpc.invoke(req, i->first.ep(), o);
		}
	}
}

void node_impl::add_router_node(udp::endpoint router)
{
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
	LIBED2K_LOG(node) << "adding router node: " << router;
#endif
	m_table.add_router_node(router);
}

void node_impl::add_node(udp::endpoint node, node_id id)
{
	// ping the node, and if we get a reply, it
	// will be added to the routing table
	void* ptr = m_rpc.allocate_observer();
	if (ptr == 0) return;

	// create a dummy traversal_algorithm		
	// this is unfortunately necessary for the observer
	// to free itself from the pool when it's being released
	boost::intrusive_ptr<traversal_algorithm> algo(
		new traversal_algorithm(*this, (node_id::min)()));
	observer_ptr o(new (ptr) null_observer(algo, node, id));
#if defined LIBED2K_DEBUG || LIBED2K_RELEASE_ASSERTS
	o->m_in_constructor = false;
#endif
    kad2_ping packet;
    m_rpc.invoke(packet, node, o);
}

void node_impl::announce(node_id const& info_hash, int listen_port
	, boost::function<void(kad_id const&)> f)
{
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
	LIBED2K_LOG(node) << "announcing [ ih: " << info_hash << " p: " << listen_port << " ]" ;
#endif
	// search for nodes with ids close to id or with peers
	// for info-hash id. then send announce_peer to them.
	boost::intrusive_ptr<find_data> ta(new find_data(*this, info_hash, f
		, boost::bind(&announce_fun, _1, boost::ref(*this)
		, listen_port, info_hash), KADEMLIA_STORE));
	ta->start();
}

void node_impl::search_keywords(node_id const& info_hash, int listen_port
  , boost::function<void(kad_id const&)> f)
{
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
    LIBED2K_LOG(node) << "search keywords [ ih: " << info_hash << " p: " << listen_port << " ]";
#endif
    // search for nodes with ids close to id or with peers
    // for info-hash id. then send announce_peer to them.
    boost::intrusive_ptr<find_data> ta(new find_data(*this, info_hash, f
    , boost::bind(&search_keywords_fun, _1, boost::ref(*this)
        , listen_port, info_hash), KADEMLIA_FIND_VALUE));
    ta->start();
}

void node_impl::search_sources(node_id const& info_hash
  , int listen_port
  , size_type size
  , boost::function<void(kad_id const&)> f)
{
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
    LIBED2K_LOG(node) << "search sources [ ih: " << info_hash << " p: " << listen_port << " ]";
#endif

    // search for nodes with ids close to id or with peers
    // for info-hash id. then send announce_peer to them.
    boost::intrusive_ptr<find_data> ta(new find_data(*this, info_hash, f
    , boost::bind(&search_sources_fun
        , _1
        , boost::ref(*this)
        , listen_port
        , size
        , info_hash), KADEMLIA_FIND_NODE));
    ta->start();
}


void node_impl::tick()
{
	node_id target;
	if (m_table.need_refresh(target))
		refresh(target, boost::bind(&nop));
}

time_duration node_impl::connection_timeout()
{
	time_duration d = m_rpc.tick();
	ptime now(time_now());
	if (now - m_last_tracker_tick < minutes(2)) return d;
	m_last_tracker_tick = now;

	for (dht_immutable_table_t::iterator i = m_immutable_table.begin();
		i != m_immutable_table.end();)
	{
		if (i->second.last_seen + minutes(60) > now)
		{
			++i;
			continue;
		}
		free(i->second.value);
		m_immutable_table.erase(i++);
	}

	// look through all peers and see if any have timed out
	for (table_t::iterator i = m_map.begin(), end(m_map.end()); i != end;)
	{
		torrent_entry& t = i->second;
		node_id const& key = i->first;
		++i;
		purge_peers(t.peers);

		// if there are no more peers, remove the entry altogether
		if (t.peers.empty())
		{
			table_t::iterator i = m_map.find(key);
			if (i != m_map.end()) m_map.erase(i);
		}
	}

	return d;
}

void node_impl::status(session_status& s)
{
	mutex_t::scoped_lock l(m_mutex);

	m_table.status(s);
	s.dht_torrents = int(m_map.size());
	s.active_requests.clear();
	s.dht_total_allocations = m_rpc.num_allocated_observers();
	for (std::set<traversal_algorithm*>::iterator i = m_running_requests.begin()
		, end(m_running_requests.end()); i != end; ++i)
	{
		s.active_requests.push_back(dht_lookup());
		dht_lookup& l = s.active_requests.back();
		(*i)->status(l);
	}
}

template<typename T>
void node_impl::incoming(const T& t, udp::endpoint target) {
    node_id id;
    if (m_rpc.incoming(t, target, &id)) refresh(id, boost::bind(&nop));
}

template void node_impl::incoming<kad2_pong>(const kad2_pong&, udp::endpoint);
template void node_impl::incoming<kad2_hello_res>(const kad2_hello_res&, udp::endpoint);
template void node_impl::incoming<kad2_bootstrap_res>(const kad2_bootstrap_res&, udp::endpoint);
template void node_impl::incoming<kademlia2_res>(const kademlia2_res&, udp::endpoint);

template<>
void node_impl::incoming_request(const kad2_ping& req, udp::endpoint target) {
    kad2_pong p;
    p.udp_port = m_port;
    udp_message msg = make_udp_message(p);
    m_send(m_userdata, msg, target, 0);
}

template<>
void node_impl::incoming_request(const kad_firewalled_req& req, udp::endpoint target) {
    kad_firewalled_res p;
    p.ip = address2int(target.address());
    udp_message msg = make_udp_message(p);
    m_send(m_userdata, msg, target, 0);
}

template<>
void node_impl::incoming_request(const kad2_hello_req& req, udp::endpoint target) {
    kad2_hello_res p;
    p.client_info.kid = m_id;
    p.client_info.tcp_port = m_port;
    p.client_info.version = KADEMLIA_VERSION;
    udp_message msg = make_udp_message(p);
    m_send(m_userdata, msg, target, 0);
    // try to add node to our table - will add if it is new node
    add_node(target, req.client_info.kid);
}

template<>
void node_impl::incoming_request(const kad2_bootstrap_req& req, udp::endpoint target) {
    kad2_bootstrap_res p;
    p.client_info.kid = m_id;
    p.client_info.tcp_port = 4661;
    p.client_info.version = KADEMLIA_VERSION;
    udp_message msg = make_udp_message(p);
    m_send(m_userdata, msg, target, 0);
}

kad_entry entry_to_emule(const node_entry& e) {
    kad_entry ke;
    ke.address.address = address2int(e.addr);
    ke.address.udp_port = e.port;
    ke.kid = e.id;
    return ke;
}

template<>
void node_impl::incoming_request(const kademlia2_req& req, udp::endpoint target) {
    kademlia2_res p;
    int count = req.search_type;
    std::vector<node_entry> res;
    m_table.find_node(req.kid_target, res, routing_table::include_failed, count);
    std::transform(res.begin(), res.end(), std::back_inserter(p.results.m_collection), boost::bind(&entry_to_emule, _1));
    udp_message msg = make_udp_message(p);
    m_send(m_userdata, msg, target, 0);
}

} } // namespace libed2k::dht

