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

#ifndef LIBED2K_DISABLE_DHT

#ifndef LIBED2K_DHT_TRACKER
#define LIBED2K_DHT_TRACKER

#include <fstream>
#include <set>
#include <numeric>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/detail/atomic_count.hpp>

#include "libed2k/kademlia/node.hpp"
#include "libed2k/kademlia/node_id.hpp"
#include "libed2k/kademlia/traversal_algorithm.hpp"
#include "libed2k/session_settings.hpp"
#include "libed2k/session_status.hpp"
#include "libed2k/udp_socket.hpp"
#include "libed2k/socket.hpp"
#include "libed2k/thread.hpp"
#include "libed2k/deadline_timer.hpp"
#include "libed2k/packet_struct.hpp"

namespace libed2k
{
	namespace aux { class session_impl; }
	struct lazy_entry;
}

namespace libed2k { namespace dht
{

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
	LIBED2K_DECLARE_LOG(dht_tracker);
#endif

	struct dht_tracker;

	LIBED2K_EXTRA_EXPORT void intrusive_ptr_add_ref(dht_tracker const*);
	LIBED2K_EXTRA_EXPORT void intrusive_ptr_release(dht_tracker const*);	

	struct dht_tracker
	{
        typedef boost::function<void(const error_code&, const char*, size_t)> dht_packet_handler;
        typedef std::map<std::pair<proto_type, proto_type>, dht_packet_handler> dht_handler_map;

		friend void intrusive_ptr_add_ref(dht_tracker const*);
		friend void intrusive_ptr_release(dht_tracker const*);
		friend bool send_callback(void* userdata, const udp_message& e, udp::endpoint const& addr, int flags);
		dht_tracker(libed2k::aux::session_impl& ses, rate_limited_udp_socket& sock
			, dht_settings const& settings, entry const* state = 0);

		void start(entry const& bootstrap);
		void stop();

		void add_node(udp::endpoint node, node_id id);
		void add_node(std::pair<std::string, int> const& node);
		void add_router_node(udp::endpoint const& node);

		entry state() const;
        kad_state estate() const;

		void announce(md4_hash const& ih, int listen_port
			, boost::function<void(kad_id const&)> f);

        void search_keywords(const md4_hash& ih, int listen_port
          , boost::function<void(kad_id const&)> f);

        void search_sources(const md4_hash& ih
          , int listen_port
          , size_type size
          , boost::function<void(kad_id const&)> f);

		void dht_status(session_status& s);
		void network_stats(int& sent, int& received);

		// translate bittorrent kademlia message into the generic kademlia message
		// used by the library
		void on_receive(udp::endpoint const& ep, char const* pkt, int size);
		void on_unreachable(udp::endpoint const& ep);

	private:

        dht_handler_map dht_request_handlers;

		boost::intrusive_ptr<dht_tracker> self()
		{ return boost::intrusive_ptr<dht_tracker>(this); }

		void on_name_lookup(error_code const& e
			, udp::resolver::iterator host);
		void on_router_name_lookup(error_code const& e
			, udp::resolver::iterator host);
		void connection_timeout(error_code const& e);
		void refresh_timeout(error_code const& e);
		void tick(error_code const& e);

		bool send_packet(const udp_message& e, udp::endpoint const& addr, int send_flags);

		node_impl m_dht;
		libed2k::aux::session_impl& m_ses;
		rate_limited_udp_socket& m_sock;

		std::vector<char> m_send_buf;

		ptime m_last_new_key;
		deadline_timer m_timer;
		deadline_timer m_connection_timer;
		deadline_timer m_refresh_timer;
		dht_settings const& m_settings;
		int m_refresh_bucket;

		bool m_abort;

		// used to resolve hostnames for nodes
		udp::resolver m_host_resolver;

		// sent and received bytes since queried last time
		int m_sent_bytes;
		int m_received_bytes;

		// used to ignore abusive dht nodes
		struct node_ban_entry
		{
			node_ban_entry(): count(0) {}
			address src;
			ptime limit;
			int count;
		};

		enum { num_ban_nodes = 20 };

		node_ban_entry m_ban_nodes[num_ban_nodes];

		// reference counter for intrusive_ptr
		mutable boost::detail::atomic_count m_refs;

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
		int m_replies_sent[5];
		int m_queries_received[5];
		int m_replies_bytes_sent[5];
		int m_queries_bytes_received[5];
		int m_counter;

		int m_total_message_input;
		int m_total_in_bytes;
		int m_total_out_bytes;
		
		int m_queries_out_bytes;
#endif
	};
}}

#endif
#endif
