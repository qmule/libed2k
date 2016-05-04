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

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
#include <fstream>
#endif

#include <set>
#include <numeric>
#include <boost/bind.hpp>
#include <boost/ref.hpp>

#include "libed2k/kademlia/node.hpp"
#include "libed2k/kademlia/node_id.hpp"
#include "libed2k/kademlia/traversal_algorithm.hpp"
#include "libed2k/kademlia/dht_tracker.hpp"
#include "libed2k/kademlia/msg.hpp"

#include "libed2k/session_impl.hpp"
#include "libed2k/socket.hpp"
#include "libed2k/socket_io.hpp"
#include "libed2k/bencode.hpp"
#include "libed2k/io.hpp"
#include "libed2k/version.hpp"
#include "libed2k/escape_string.hpp"

#define MINIZ_HEADER_FILE_ONLY
#include "../miniz.c"

using boost::ref;
using libed2k::dht::node_impl;
using libed2k::dht::node_id;
using libed2k::dht::packet_t;
using libed2k::dht::msg;
using namespace libed2k::detail;

enum
{
	key_refresh = 5 // generate a new write token key every 5 minutes
};

namespace
{
	const int tick_period = 1; // minutes

	template <class EndpointType>
	void read_endpoint_list(libed2k::entry const* n, std::vector<EndpointType>& epl)
	{
		using namespace libed2k;
		if (n->type() != entry::list_t) return;
		entry::list_type const& contacts = n->list();
		for (entry::list_type::const_iterator i = contacts.begin()
			, end(contacts.end()); i != end; ++i)
		{
			if (i->type() != entry::string_t) return;
			std::string const& p = i->string();
			if (p.size() < 6) continue;
			std::string::const_iterator in = p.begin();
			if (p.size() == 6)
				epl.push_back(read_v4_endpoint<EndpointType>(in));
#if LIBED2K_USE_IPV6
			else if (p.size() == 18)
				epl.push_back(read_v6_endpoint<EndpointType>(in));
#endif
		}
	}

}

namespace libed2k { namespace dht
{

	void incoming_error(entry& e, char const* msg);

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
	int g_az_message_input = 0;
	int g_ut_message_input = 0;
	int g_lt_message_input = 0;
	int g_mp_message_input = 0;
	int g_gr_message_input = 0;
	int g_mo_message_input = 0;
	int g_unknown_message_input = 0;

	int g_announces = 0;
	int g_failed_announces = 0;
#endif
		
	void intrusive_ptr_add_ref(dht_tracker const* c)
	{
		LIBED2K_ASSERT(c != 0);
		LIBED2K_ASSERT(c->m_refs >= 0);
		++c->m_refs;
	}

	void intrusive_ptr_release(dht_tracker const* c)
	{
		LIBED2K_ASSERT(c != 0);
		LIBED2K_ASSERT(c->m_refs > 0);
		if (--c->m_refs == 0)
			delete c;
	}

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
	LIBED2K_DEFINE_LOG(dht_tracker)
#endif

	node_id extract_node_id(lazy_entry const* e)
	{
		if (e == 0 || e->type() != lazy_entry::dict_t) return (node_id::min)();
		lazy_entry const* nid = e->dict_find_string("node-id");
		if (nid == 0 || nid->string_length() != 20) return (node_id::min)();
		return node_id(node_id(nid->string_ptr()));
	}

	node_id extract_node_id(entry const* e)
	{
		if (e == 0 || e->type() != entry::dictionary_t) return (node_id::min)();
		entry const* nid = e->find_key("node-id");
		if (nid == 0 || nid->type() != entry::string_t || nid->string().length() != 20)
			return (node_id::min)();
		return node_id(node_id(nid->string().c_str()));
	}

	bool send_callback(void* userdata, const udp_message& e, udp::endpoint const& addr, int flags)
	{
		dht_tracker* self = (dht_tracker*)userdata;
		return self->send_packet(e, addr, flags);
	}

	// class that puts the networking and the kademlia node in a single
	// unit and connecting them together.
	dht_tracker::dht_tracker(libed2k::aux::session_impl& ses, rate_limited_udp_socket& sock
		, dht_settings const& settings, entry const* state)
		: m_dht(ses.m_alerts, &send_callback, settings, extract_node_id(state)
			, ses.external_address()
			, ses.listen_port()
			, boost::bind(&aux::session_impl::set_external_address, &ses, _1, _2, _3)
			, this)
		, m_ses(ses)
		, m_sock(sock)
		, m_last_new_key(time_now() - minutes(key_refresh))
		, m_timer(sock.get_io_service())
		, m_connection_timer(sock.get_io_service())
		, m_refresh_timer(sock.get_io_service())
		, m_settings(settings)
		, m_refresh_bucket(160)
		, m_abort(false)
		, m_host_resolver(sock.get_io_service())
		, m_sent_bytes(0)
		, m_received_bytes(0)
		, m_refs(0)
	{
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
		m_counter = 0;
		std::fill_n(m_replies_bytes_sent, 5, 0);
		std::fill_n(m_queries_bytes_received, 5, 0);
		std::fill_n(m_replies_sent, 5, 0);
		std::fill_n(m_queries_received, 5, 0);
		g_announces = 0;
		g_failed_announces = 0;
		m_total_message_input = 0;
		m_total_in_bytes = 0;
		m_total_out_bytes = 0;
		m_queries_out_bytes = 0;
		
		// turns on and off individual components' logging

//		rpc_log().enable(false);
//		node_log().enable(false);
//		traversal_log().enable(false);
//		dht_tracker_log.enable(false);

		LIBED2K_LOG(dht_tracker) << "starting DHT tracker with node id: " << m_dht.nid();
#endif
	}

	// defined in node.cpp
	extern void nop();

	void dht_tracker::start(entry const& bootstrap)
	{
		LIBED2K_ASSERT(m_ses.is_network_thread());
		std::vector<udp::endpoint> initial_nodes;

		if (bootstrap.type() == entry::dictionary_t)
		{
			LIBED2K_TRY {
				if (entry const* nodes = bootstrap.find_key("nodes"))
					read_endpoint_list<udp::endpoint>(nodes, initial_nodes);
			} LIBED2K_CATCH(std::exception&) {}
		}

		error_code ec;
		m_timer.expires_from_now(seconds(1), ec);
		m_timer.async_wait(boost::bind(&dht_tracker::tick, self(), _1));

		m_connection_timer.expires_from_now(seconds(1), ec);
		m_connection_timer.async_wait(
			boost::bind(&dht_tracker::connection_timeout, self(), _1));

		m_refresh_timer.expires_from_now(seconds(5), ec);
		m_refresh_timer.async_wait(boost::bind(&dht_tracker::refresh_timeout, self(), _1));
		m_dht.bootstrap(initial_nodes, boost::bind(&libed2k::dht::nop));
	}

	void dht_tracker::stop()
	{
		LIBED2K_ASSERT(m_ses.is_network_thread());
		m_abort = true;
		error_code ec;
		m_timer.cancel(ec);
		m_connection_timer.cancel(ec);
		m_refresh_timer.cancel(ec);
		m_host_resolver.cancel();
	}

	void dht_tracker::dht_status(session_status& s)
	{
		LIBED2K_ASSERT(m_ses.is_network_thread());
		m_dht.status(s);
	}

	void dht_tracker::network_stats(int& sent, int& received)
	{
		LIBED2K_ASSERT(m_ses.is_network_thread());
		sent = m_sent_bytes;
		received = m_received_bytes;
		m_sent_bytes = 0;
		m_received_bytes = 0;
	}

	void dht_tracker::connection_timeout(error_code const& e)
	{
		LIBED2K_ASSERT(m_ses.is_network_thread());
		if (e || m_abort) return;

		time_duration d = m_dht.connection_timeout();
		error_code ec;
		m_connection_timer.expires_from_now(d, ec);
		m_connection_timer.async_wait(boost::bind(&dht_tracker::connection_timeout, self(), _1));
	}

	void dht_tracker::refresh_timeout(error_code const& e)
	{
		LIBED2K_ASSERT(m_ses.is_network_thread());
		if (e || m_abort) return;

		m_dht.tick();
		error_code ec;
		m_refresh_timer.expires_from_now(seconds(5), ec);
		m_refresh_timer.async_wait(
			boost::bind(&dht_tracker::refresh_timeout, self(), _1));
	}

	void dht_tracker::tick(error_code const& e)
	{
		LIBED2K_ASSERT(m_ses.is_network_thread());
		if (e || m_abort) return;

		error_code ec;
		m_timer.expires_from_now(minutes(tick_period), ec);
		m_timer.async_wait(boost::bind(&dht_tracker::tick, self(), _1));

		ptime now = time_now();
		if (now - m_last_new_key > minutes(key_refresh))
		{
			m_last_new_key = now;
			m_dht.new_write_key();
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
			LIBED2K_LOG(dht_tracker) << " *** new write key";
#endif
		}
		
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
		static bool first = true;

		std::ofstream st("dht_routing_table_state.txt", std::ios_base::trunc);
		m_dht.print_state(st);
		
		// count torrents
		int torrents = m_dht.num_torrents();
		
		// count peers
		int peers = m_dht.num_peers();

		std::ofstream pc("dht_stats.log", first ? std::ios_base::trunc : std::ios_base::app);
		if (first)
		{
			first = false;
			pc << "\n\n *****   starting log at " << time_now_string() << "   *****\n\n"
				<< "minute:active nodes:passive nodes"
				":ping replies sent:ping queries recvd"
				":ping replies bytes sent:ping queries bytes recvd"
				":find_node replies sent:find_node queries recv"
				":find_node replies bytes sent:find_node queries bytes recv"
				":get_peers replies sent:get_peers queries recvd"
				":get_peers replies bytes sent:get_peers queries bytes recv"
				":announce_peer replies sent:announce_peer queries recvd"
				":announce_peer replies bytes sent:announce_peer queries bytes recv"
				":error replies sent:error queries recvd"
				":error replies bytes sent:error queries bytes recv"
				":num torrents:num peers:announces per min"
				":failed announces per min:total msgs per min"
				":az msgs per min:ut msgs per min:lt msgs per min:mp msgs per min"
				":gr msgs per min:mo msgs per min:bytes in per sec:bytes out per sec"
				":queries out bytes per sec\n\n";
		}

		int active;
		int passive;
		boost::tie(active, passive) = m_dht.size();
		pc << (m_counter * tick_period)
			<< "\t" << active
			<< "\t" << passive;
		for (int i = 0; i < 5; ++i)
			pc << "\t" << (m_replies_sent[i] / float(tick_period))
				<< "\t" << (m_queries_received[i] / float(tick_period))
				<< "\t" << (m_replies_bytes_sent[i] / float(tick_period*60))
				<< "\t" << (m_queries_bytes_received[i] / float(tick_period*60));
		
		pc << "\t" << torrents
			<< "\t" << peers
			<< "\t" << g_announces / float(tick_period)
			<< "\t" << g_failed_announces / float(tick_period)
			<< "\t" << (m_total_message_input / float(tick_period))
			<< "\t" << (g_az_message_input / float(tick_period))
			<< "\t" << (g_ut_message_input / float(tick_period))
			<< "\t" << (g_lt_message_input / float(tick_period))
			<< "\t" << (g_mp_message_input / float(tick_period))
			<< "\t" << (g_gr_message_input / float(tick_period))
			<< "\t" << (g_mo_message_input / float(tick_period))
			<< "\t" << (m_total_in_bytes / float(tick_period*60))
			<< "\t" << (m_total_out_bytes / float(tick_period*60))
			<< "\t" << (m_queries_out_bytes / float(tick_period*60))
			<< std::endl;
		++m_counter;
		std::fill_n(m_replies_bytes_sent, 5, 0);
		std::fill_n(m_queries_bytes_received, 5, 0);
		std::fill_n(m_replies_sent, 5, 0);
		std::fill_n(m_queries_received, 5, 0);
		g_announces = 0;
		g_failed_announces = 0;
		m_total_message_input = 0;
		g_az_message_input = 0;
		g_ut_message_input = 0;
		g_lt_message_input = 0;
		g_mp_message_input = 0;
		g_gr_message_input = 0;
		g_mo_message_input = 0;
		g_unknown_message_input = 0;
		m_total_in_bytes = 0;
		m_total_out_bytes = 0;
		m_queries_out_bytes = 0;
#endif
	}

	void dht_tracker::announce(md4_hash const& ih, int listen_port
		, boost::function<void(std::vector<tcp::endpoint> const&)> f)
	{
		LIBED2K_ASSERT(m_ses.is_network_thread());
		m_dht.announce(ih, listen_port, f);
	}

  void dht_tracker::search_keywords(md4_hash& ih, int listen_port
    , boost::function<void(std::vector<tcp::endpoint> const&)> f) {
    LIBED2K_ASSERT(m_ses.is_network_thread());
    m_dht.search_keywords(ih, listen_port, f);
  }

  void dht_tracker::search_sources(const md4_hash& ih
    , int listen_port
    , size_type size
    , boost::function<void(std::vector<tcp::endpoint> const&)> f) {
    LIBED2K_ASSERT(m_ses.is_network_thread());
    m_dht.search_sources(ih, listen_port, size, f);
  }


	void dht_tracker::on_unreachable(udp::endpoint const& ep)
	{
		LIBED2K_ASSERT(m_ses.is_network_thread());
		m_dht.unreachable(ep);
	}

	// translate eDonkey kademlia message into the generic kademlia packet
	// used by the library
	void dht_tracker::on_receive(udp::endpoint const& ep, char const* buf, int bytes_transferred)
	{
        LIBED2K_ASSERT(m_ses.is_network_thread());

        error_code ec;
        udp_libed2k_header uh;

        // bytes count should be at least as packet header size
        if (bytes_transferred < sizeof(udp_libed2k_header)) {
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
            LIBED2K_LOG(dht_tracker) << " message extract error: incoming udp message too short";
#endif
            return;
        }

        uh = *(reinterpret_cast<const udp_libed2k_header*>(buf));

        if (uh.m_protocol != OP_KADEMLIAHEADER && uh.m_protocol != OP_KADEMLIAPACKEDPROT) {
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
            LIBED2K_LOG(dht_tracker) << " packet protocol type is not KAD: " << std::hex << (int)uh.m_protocol << " from " << ep.address();
#endif 
            return;
        }

        std::vector<uint8_t> container;

        if (uh.m_protocol == OP_KADEMLIAPACKEDPROT) {
                // unzip data
                container.resize(bytes_transferred * 10 + 300);
                uLongf nSize = container.size();
                int nRet = uncompress((Bytef*)&container[0], &nSize, (const Bytef*)(buf + 2), bytes_transferred - 2);

                if (nRet != Z_OK)
                {
                    ERR("Unzip error: " << mz_error(nRet));
                    return;
                }

                uh.m_protocol = OP_KADEMLIAHEADER;
                container.resize(nSize);
        }
        else {
            container.assign(buf + 2, buf + bytes_transferred);
        }

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
        if (container.empty()) {
            LIBED2K_LOG(dht_tracker) << " incoming data: empty(only header)";
            return;
        }
        else {
            std::string cincoming((char*)&container[0], container.size());
            LIBED2K_LOG(dht_tracker) << " incoming data: " << to_hex(cincoming);
        }
#endif
        // TODO - need total fix for serialization
        const char* incoming = NULL;
        if (!container.empty()) incoming = (const char*)&container[0];

        typedef boost::iostreams::basic_array_source<char> Device;
        boost::iostreams::stream_buffer<Device> buffer(incoming, container.size());
        std::istream in_array_stream(&buffer);
        archive::ed2k_iarchive ia(in_array_stream);

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
        LIBED2K_LOG(dht_tracker) << kad2string(uh.m_type) << " <== " << ep.address();
#endif
        /**
          * incoming requests
        */
        switch (uh.m_type) {
        case KADEMLIA_BOOTSTRAP_REQ_DEPRECATED:
        case KADEMLIA_HELLO_REQ_DEPRECATED:
        case KADEMLIA_REQ_DEPRECATED: {
            kademlia_req req;
            ia >> req;
            break;
        }

        case KADEMLIA_SEARCH_REQ:
        case KADEMLIA_SEARCH_NOTES_REQ:
        case KADEMLIA_PUBLISH_REQ:
        case KADEMLIA_PUBLISH_NOTES_REQ_DEPRECATED:
            break;
        case KADEMLIA_FIREWALLED_REQ: {
            kad_firewalled_req p;
            ia >> p;
            m_dht.incoming_request(p, ep);
            break;
        }
        case KADEMLIA_FINDBUDDY_REQ:
            break;
        case KADEMLIA_CALLBACK_REQ:
            break;
        case KADEMLIA2_BOOTSTRAP_REQ: {
            kad2_bootstrap_req p;
            ia >> p;
            m_dht.incoming_request(p, ep);
            break;
        }
        case KADEMLIA2_BOOTSTRAP_RES: {
            kad2_bootstrap_res p;
            ia >> p;
            m_dht.incoming(p, ep);
            break;
        }
        case KADEMLIA2_HELLO_REQ: {
            kad2_hello_req p;
            ia >> p;
            m_dht.incoming_request(p, ep);
            break;
        }
        case KADEMLIA2_HELLO_RES: {
            kad2_hello_res p;
            ia >> p;
            m_dht.incoming(p, ep);
            break;
        }
        case KADEMLIA2_REQ: {
            kademlia2_req p;
            ia >> p;
            m_dht.incoming_request(p, ep);
            break;
        }
        case KADEMLIA2_HELLO_RES_ACK: {
            break;
        }
        case KADEMLIA2_RES: {
            kademlia2_res p;
            ia >> p;
            m_dht.incoming(p, ep);
            break;
        }
        case KADEMLIA2_SEARCH_KEY_REQ: {
            break;
        }
        case KADEMLIA2_SEARCH_SOURCE_REQ: {
            break;
        }
        case KADEMLIA2_SEARCH_NOTES_REQ: {
            break;
        }
        case KADEMLIA2_SEARCH_RES: {
            /*
            Search types and tags:

            File search:
            uint8 uType = 0;
            uint32 uIP = 0;
            uint16 uTCPPort = 0;
            uint16 uUDPPort = 0;
            uint32 uBuddyIP = 0;
            uint16 uBuddyPort = 0;
            //uint32 uClientID = 0;
            CUInt128 uBuddy;
            uint8 byCryptOptions = 0; // 0 = not supported

            for (TagList::const_iterator itTagList = plistInfo->begin(); itTagList != plistInfo->end(); ++itTagList)
            {
            CKadTag* pTag = *itTagList;
            if (!pTag->m_name.Compare(TAG_SOURCETYPE))
            uType = (uint8)pTag->GetInt();
            else if (!pTag->m_name.Compare(TAG_SOURCEIP))
            uIP = (uint32)pTag->GetInt();
            else if (!pTag->m_name.Compare(TAG_SOURCEPORT))
            uTCPPort = (uint16)pTag->GetInt();
            else if (!pTag->m_name.Compare(TAG_SOURCEUPORT))
            uUDPPort = (uint16)pTag->GetInt();
            else if (!pTag->m_name.Compare(TAG_SERVERIP))
            uBuddyIP = (uint32)pTag->GetInt();
            else if (!pTag->m_name.Compare(TAG_SERVERPORT))
            uBuddyPort = (uint16)pTag->GetInt();
            //else if (!pTag->m_name.Compare(TAG_CLIENTLOWID))
            //  uClientID = pTag->GetInt();
            else if (!pTag->m_name.Compare(TAG_BUDDYHASH))
            {
            uchar ucharBuddyHash[16];
            if (pTag->IsStr() && strmd4(pTag->GetStr(), ucharBuddyHash))
            md4cpy(uBuddy.GetDataPtr(), ucharBuddyHash);
            else
            TRACE("+++ Invalid TAG_BUDDYHASH tag\n");
            }
          
            Keywords search:

            1488[dbg] {tag: TAGTYPE_STRING} {name: } {id: FT_FILENAME} {val: "Lady Gaga - Love Game.mp3"}
            1489[dbg] {tag: TAGTYPE_UINT32} {name: } {id: FT_FILESIZE} {val: 4560868}
            1490[dbg] {tag: TAGTYPE_UINT8} {name: } {id: FT_SOURCES} {val: 1}
            1491[dbg] {tag: TAGTYPE_STRING} {name: } {id: FT_FILETYPE} {val: "Audio"}
            1492[dbg] {tag: TAGTYPE_STRING} {name: } {id: FT_MEDIA_ALBUM} {val: "The Fame"}
            1493[dbg] {tag: TAGTYPE_UINT8} {name: } {id: FT_MEDIA_LENGTH} {val: 214}
            1494[dbg] {tag: TAGTYPE_UINT8} {name: } {id: FT_MEDIA_BITRATE} {val: 170}
            1495[dbg] {tag: TAGTYPE_UINT32} {name: } {id: FT_PUBLISHINFO} {val: 33686170}
          
            */


            kad2_search_res p;
            DBG("start search res parsing");
            ia >> p;
            DBG("search res for " << p.target_id << " count " << p.results.m_collection.size());
            for (std::deque<kad_info_entry>::const_iterator itr = p.results.m_collection.begin(); itr != p.results.m_collection.end(); ++itr) {
                DBG("answer " << itr->hash);
                itr->tags.dump();
            }

            if (!p.results.m_collection.empty()) {
                // probe result type
                if (p.results.m_collection.front().tags.getTagByNameId(TAG_SOURCETYPE)) {
                    // sources answer
                    for (std::deque<kad_info_entry>::const_iterator itr = p.results.m_collection.begin(); itr != p.results.m_collection.end(); ++itr) {
                        md4_hash h = p.target_id;
                        m_ses.on_find_dht_source(h
                            , itr->tags.getIntTagByNameId(TAG_SOURCETYPE)
                            , ntohl(itr->tags.getIntTagByNameId(TAG_SOURCEIP))
                            , itr->tags.getIntTagByNameId(TAG_SOURCEPORT)
                            , itr->tags.getIntTagByNameId(TAG_CLIENTLOWID));
                    }
                }
                else {
                    // now it is always keywords result
                    m_ses.on_find_dht_keyword(p.target_id, p.results.m_collection);
                }
            }
            break;
        }
        case KADEMLIA2_PUBLISH_KEY_REQ: {
            break;
        }
        case KADEMLIA2_PUBLISH_SOURCE_REQ: {

            break;
        }
        case KADEMLIA2_PUBLISH_NOTES_REQ: {
            break;
        }
        case KADEMLIA2_PUBLISH_RES: {
            break;
        }
        case KADEMLIA2_PUBLISH_RES_ACK: {
            break;
        }
        case KADEMLIA_FIREWALLED2_REQ: {
            break;
        }
        case KADEMLIA2_PING: {
            kad2_ping p;
            ia >> p;
            m_dht.incoming_request(p, ep);
            break;
        }
        case KADEMLIA2_PONG: {
            kad2_pong p;
            ia >> p;
            m_dht.incoming(p, ep);
            break;
        }

        case KADEMLIA2_FIREWALLUDP: {
            break;
        }
        default: {
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
            LIBED2K_LOG(dht_tracker) << "not handled packet type " << uh.m_type << " <<< " << ep.address();
#endif
            break;
        }
        };

		// account for IP and UDP overhead
		m_received_bytes += bytes_transferred + (ep.address().is_v6() ? 48 : 28);

		node_ban_entry* match = 0;
		node_ban_entry* min = m_ban_nodes;
		ptime now = time_now();
		for (node_ban_entry* i = m_ban_nodes; i < m_ban_nodes + num_ban_nodes; ++i)
		{
			if (i->src == ep.address())
			{
				match = i;
				break;
			}
			if (i->count < min->count) min = i;
		}

		if (match)
		{
			++match->count;
			if (match->count >= 20)
			{
				if (now < match->limit)
				{
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
					if (match->count == 20)
					{
						LIBED2K_LOG(dht_tracker) << " BANNING PEER [ ip: "
							<< ep << " time: " << total_milliseconds((now - match->limit) + seconds(5)) / 1000.f
							<< " count: " << match->count << " ]";
					}
#endif
					// we've received 20 messages in less than 5 seconds from
					// this node. Ignore it until it's silent for 5 minutes
					match->limit = now + minutes(5);
					return;
				}

				// we got 50 messages from this peer, but it was in
				// more than 5 seconds. Reset the counter and the timer
				match->count = 0;
				match->limit = now + seconds(5);
			}
		}
		else
		{
			min->count = 1;
			min->limit = now + seconds(5);
			min->src = ep.address();
		}

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
		++m_total_message_input;
		m_total_in_bytes += bytes_transferred;
#endif
	}

	void add_node_fun(void* userdata, node_entry const& e)
	{
		entry* n = (entry*)userdata;
		std::string node;
		std::back_insert_iterator<std::string> out(node);
		write_endpoint(e.ep(), out);
		n->list().push_back(entry(node));
	}
	
	entry dht_tracker::state() const
	{
		LIBED2K_ASSERT(m_ses.is_network_thread());
		entry ret(entry::dictionary_t);
		{
			entry nodes(entry::list_t);
			m_dht.m_table.for_each_node(&add_node_fun, &add_node_fun, &nodes);
			bucket_t cache;
			m_dht.replacement_cache(cache);
			for (bucket_t::iterator i(cache.begin())
				, end(cache.end()); i != end; ++i)
			{
				std::string node;
				std::back_insert_iterator<std::string> out(node);
				write_endpoint(udp::endpoint(i->addr, i->port), out);
				nodes.list().push_back(entry(node));
			}
			if (!nodes.list().empty())
				ret["nodes"] = nodes;
		}

		ret["node-id"] = m_dht.nid().to_string();
		return ret;
	}

	void dht_tracker::add_node(udp::endpoint node, node_id id)
	{
		LIBED2K_ASSERT(m_ses.is_network_thread());
		m_dht.add_node(node, id);
	}

	void dht_tracker::add_node(std::pair<std::string, int> const& node)
	{
		LIBED2K_ASSERT(m_ses.is_network_thread());
		char port[7];
		snprintf(port, sizeof(port), "%d", node.second);
		udp::resolver::query q(node.first, port);
		m_host_resolver.async_resolve(q,
			boost::bind(&dht_tracker::on_name_lookup, self(), _1, _2));
	}

	void dht_tracker::on_name_lookup(error_code const& e
		, udp::resolver::iterator host)
	{
		LIBED2K_ASSERT(m_ses.is_network_thread());
		if (e || host == udp::resolver::iterator()) return;
		add_node(host->endpoint(), node_id(0));
	}

	void dht_tracker::add_router_node(udp::endpoint const& node)
	{
		LIBED2K_ASSERT(m_ses.is_network_thread());
		m_dht.add_router_node(node);
	}

	bool dht_tracker::send_packet(const udp_message& e, udp::endpoint const& addr, int send_flags)
	{
		LIBED2K_ASSERT(m_ses.is_network_thread());

		m_send_buf.clear();
		error_code ec;
        
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
		std::stringstream log_line;
		//lazy_entry print;
		//int ret = lazy_bdecode(&m_send_buf[0], &m_send_buf[0] + m_send_buf.size(), print, ec);
		//LIBED2K_ASSERT(ret == 0);
		//log_line << print_entry(print, true);
        log_line << kad2string(e.first.m_type) << " size " << e.second.size();
#endif
        std::copy((const char*)&(e.first), (const char*)(&(e.first)) + sizeof(e.first), std::back_inserter(m_send_buf));
        std::copy(e.second.begin(), e.second.end(), std::back_inserter(m_send_buf));
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
        log_line << " data size " << m_send_buf.size();
#endif

		if (m_sock.send(addr, &m_send_buf[0], (int)m_send_buf.size(), ec, send_flags))
		{
			if (ec) return false;

			// account for IP and UDP overhead
			m_sent_bytes += m_send_buf.size() + (addr.address().is_v6() ? 48 : 28);

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
			m_total_out_bytes += m_send_buf.size();
			LIBED2K_LOG(dht_tracker) << "==> " << addr << " " << log_line.str();
#endif
			return true;
		}
		else
		{
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
			LIBED2K_LOG(dht_tracker) << "==> " << addr << " DROPPED " << log_line.str();
#endif
			return false;
		}
	}

}}

