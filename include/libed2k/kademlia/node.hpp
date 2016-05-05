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

#ifndef NODE_HPP
#define NODE_HPP

#include <algorithm>
#include <map>
#include <set>

#include <libed2k/config.hpp>
#include <libed2k/kademlia/routing_table.hpp>
#include <libed2k/kademlia/rpc_manager.hpp>
#include <libed2k/kademlia/node_id.hpp>
#include <libed2k/kademlia/msg.hpp>
#include <libed2k/kademlia/find_data.hpp>

#include <libed2k/io.hpp>
#include <libed2k/session_settings.hpp>
#include <libed2k/assert.hpp>
#include <libed2k/thread.hpp>
#include <libed2k/bloom_filter.hpp>
#include "libed2k/packet_struct.hpp"

#include <boost/cstdint.hpp>
#include <boost/ref.hpp>

#include "libed2k/socket.hpp"

namespace libed2k {
	class alert_manager;
}

namespace libed2k { namespace dht
{

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
LIBED2K_DECLARE_LOG(node);
#endif

struct traversal_algorithm;

struct key_desc_t
{
	char const* name;
	int type;
	int size;
	int flags;

	enum {
		// this argument is optional, parsing will not
		// fail if it's not present
		optional = 1,
		// for dictionaries, the following entries refer
		// to child nodes to this node, up until and including
		// the next item that has the last_child flag set.
		// these flags are nestable
		parse_children = 2,
		// this is the last item in a child dictionary
		last_child = 4,
		// the size argument refers to that the size
		// has to be divisible by the number, instead
		// of having that exact size
		size_divisible = 8
	}; 
};

bool LIBED2K_EXTRA_EXPORT verify_message(lazy_entry const* msg, key_desc_t const desc[]
	, lazy_entry const* ret[], int size , char* error, int error_size);

// this is the entry for every peer
// the timestamp is there to make it possible
// to remove stale peers
struct peer_entry
{
	tcp::endpoint addr;
	ptime added;
	bool seed;
};

// this is a group. It contains a set of group members
struct torrent_entry
{
	std::string name;
	std::set<peer_entry> peers;
};

struct dht_immutable_item
{
	dht_immutable_item() : value(0), num_announcers(0), size(0) {}
	// malloced space for the actual value
	char* value;
	// this counts the number of IPs we have seen
	// announcing this item, this is used to determine
	// popularity if we reach the limit of items to store
	bloom_filter<128> ips;
	// the last time we heard about this
	ptime last_seen;
	// number of IPs in the bloom filter
	int num_announcers;
	// size of malloced space pointed to by value
	int size;
};

struct rsa_key { char bytes[268]; };

struct dht_mutable_item : dht_immutable_item
{
	char sig[256];
	int seq;
	rsa_key key;
};

inline bool operator<(rsa_key const& lhs, rsa_key const& rhs)
{
	return memcmp(lhs.bytes, rhs.bytes, sizeof(lhs.bytes)) < 0;
}

inline bool operator<(peer_entry const& lhs, peer_entry const& rhs)
{
	return lhs.addr.address() == rhs.addr.address()
		? lhs.addr.port() < rhs.addr.port()
		: lhs.addr.address() < rhs.addr.address();
}

struct null_type {};

class announce_observer : public observer
{
public:
	announce_observer(boost::intrusive_ptr<traversal_algorithm> const& algo
		, udp::endpoint const& ep, node_id const& id)
		: observer(algo, ep, id)
	{}


  virtual void reply(const kad2_pong&, udp::endpoint ep) { flags |= flag_done; }
  virtual void reply(const kad2_hello_res&, udp::endpoint ep) { flags |= flag_done; }
  virtual void reply(const kad2_bootstrap_res&, udp::endpoint ep) { flags |= flag_done; }
  virtual void reply(const kademlia2_res&, udp::endpoint ep) { flags |= flag_done; }
};

struct count_peers
{
	int& count;
	count_peers(int& c): count(c) {}
	void operator()(std::pair<libed2k::dht::node_id
		, libed2k::dht::torrent_entry> const& t)
	{
		count += t.second.peers.size();
	}
};
	
class LIBED2K_EXTRA_EXPORT node_impl : boost::noncopyable
{
typedef std::map<node_id, torrent_entry> table_t;
typedef std::map<node_id, dht_immutable_item> dht_immutable_table_t;
typedef std::map<node_id, dht_mutable_item> dht_mutable_table_t;

public:
	typedef boost::function3<void, address, int, address> external_ip_fun;

	node_impl(libed2k::alert_manager& alerts
		, bool (*f)(void*, const udp_message&, udp::endpoint const&, int)
		, dht_settings const& settings, node_id nid, address const& external_address, uint16_t port
		, external_ip_fun ext_ip, void* userdata);

	virtual ~node_impl() {}

	void tick();
	void refresh(node_id const& id, find_data::nodes_callback const& f);
	void bootstrap(std::vector<udp::endpoint> const& nodes
		, find_data::nodes_callback const& f);
	void add_router_node(udp::endpoint router);
		
	void unreachable(udp::endpoint const& ep);

	template<typename T>
	void incoming(const T& t, udp::endpoint target);    

	int num_torrents() const { return m_map.size(); }
	int num_peers() const
	{
		int ret = 0;
		std::for_each(m_map.begin(), m_map.end(), count_peers(ret));
		return ret;
	}

	int bucket_size(int bucket);

	node_id const& nid() const { return m_id; }

	boost::tuple<int, int> size() const{ return m_table.size(); }
	size_type num_global_nodes() const
	{ return m_table.num_global_nodes(); }

	int data_size() const { return int(m_map.size()); }

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
	void print_state(std::ostream& os) const
	{ m_table.print_state(os); }
#endif

	void announce(node_id const& info_hash, int listen_port
		, boost::function<void(std::vector<tcp::endpoint> const&)> f);

  void search_keywords(node_id const& info_hash, int listen_port
    , boost::function<void(std::vector<tcp::endpoint> const&)> f);

  void search_sources(node_id const& info_hash
    , int listen_port
    , size_type size
    , boost::function<void(std::vector<tcp::endpoint> const&)> f);

	// the returned time is the delay until connection_timeout()
	// should be called again the next time
	time_duration connection_timeout();

	// pings the given node, and adds it to
	// the routing table if it respons and if the
	// bucket is not full.
	void add_node(udp::endpoint node, node_id id);

	void replacement_cache(bucket_t& nodes) const
	{ m_table.replacement_cache(nodes); }

	int branch_factor() const { return m_settings.search_branching; }

	void add_traversal_algorithm(traversal_algorithm* a)
	{
		mutex_t::scoped_lock l(m_mutex);
		m_running_requests.insert(a);
	}

	void remove_traversal_algorithm(traversal_algorithm* a)
	{
		mutex_t::scoped_lock l(m_mutex);
		m_running_requests.erase(a);
	}

	void status(libed2k::session_status& s);

	dht_settings const& settings() const { return m_settings; }

protected:

	void lookup_peers(md4_hash const& info_hash, int prefix, entry& reply
		, bool noseed, bool scrape) const;
	bool lookup_torrents(sha1_hash const& target, entry& reply
		, char* tags) const;

	dht_settings const& m_settings;
	
private:
	typedef libed2k::mutex mutex_t;
	mutex_t m_mutex;

	// this list must be destructed after the rpc manager
	// since it might have references to it
	std::set<traversal_algorithm*> m_running_requests;

	node_id m_id;

public:
	routing_table m_table;
	rpc_manager m_rpc;

    template<typename Request>
    void incoming_request(const Request& req, udp::endpoint target);

private:
	external_ip_fun m_ext_ip;

	table_t m_map;
	dht_immutable_table_t m_immutable_table;
	dht_mutable_table_t m_mutable_table;
	
	ptime m_last_tracker_tick;

	libed2k::alert_manager& m_alerts;
	bool (*m_send)(void*, const udp_message&, udp::endpoint const&, int);
	void* m_userdata;
	uint16_t m_port;
};


} } // namespace libed2k::dht

#endif // NODE_HPP

