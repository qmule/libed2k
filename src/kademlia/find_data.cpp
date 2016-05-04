/*

Copyright (c) 2006, Arvid Norberg & Daniel Wallin
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

#include <libed2k/kademlia/find_data.hpp>
#include <libed2k/kademlia/routing_table.hpp>
#include <libed2k/kademlia/rpc_manager.hpp>
#include <libed2k/kademlia/node.hpp>
#include <libed2k/io.hpp>
#include <libed2k/socket.hpp>
#include <libed2k/socket_io.hpp>
#include "libed2k/util.hpp"
#include <vector>

namespace libed2k { namespace dht
{

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
	LIBED2K_DECLARE_LOG(traversal);
#endif

using detail::read_endpoint_list;
using detail::read_v4_endpoint;
#if LIBED2K_USE_IPV6
using detail::read_v6_endpoint;
#endif

/**
 * emule code for extract information about source from file
 *  // Process a possible source to a file.
    // Set of data we could receive from the result.
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
        else if (!pTag->m_name.Compare(TAG_ENCRYPTION))
            byCryptOptions = (uint8)pTag->GetInt();

        delete pTag;
    }
    delete plistInfo;

    // Process source based on it's type. Currently only one method is needed to process all types.
    switch( uType )
    {
        case 1:
        case 3:
        case 4:
        case 5:
        case 6:
            m_uAnswers++;
            theApp.emuledlg->kademliawnd->searchList->SearchRef(this);
            theApp.downloadqueue->KademliaSearchFile(m_uSearchID, &uAnswer, &uBuddy, uType, uIP, uTCPPort, uUDPPort, uBuddyIP, uBuddyPort, byCryptOptions);
            break;
    }
*/

void find_data_observer::reply(const kad2_pong& r, udp::endpoint ep) { done(); }
void find_data_observer::reply(const kad2_hello_res& r, udp::endpoint ep) { done(); }

void find_data_observer::reply(const kad2_bootstrap_res& r, udp::endpoint ep) {

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
    LIBED2K_LOG(traversal) << "kad2_bootstrap reply " << m_algorithm.get() << " count " << r.contacts.m_collection.size();
#endif

    for (kad_contacts_res::const_iterator itr = r.contacts.m_collection.begin(); itr != r.contacts.m_collection.end(); ++itr) {    
        error_code ec;
        ip::address addr = ip::address::from_string(int2ipstr(itr->address.address), ec);
        if (!ec) {
    #ifdef LIBED2K_DHT_VERBOSE_LOGGING
            LIBED2K_LOG(traversal) << "traverse " << itr->kid << " " << int2ipstr(itr->address.address);
            m_algorithm->traverse(itr->kid, udp::endpoint(addr, itr->address.udp_port));
    #endif
        }
    }

    done();
}

void find_data_observer::reply(const kademlia2_res& r, udp::endpoint ep) {
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
    LIBED2K_LOG(traversal) << "kademlia2_res reply " << m_algorithm.get() << " count " << r.results.m_collection.size();
#endif

    for (kad_contacts_res::const_iterator itr = r.results.m_collection.begin(); itr != r.results.m_collection.end(); ++itr) {
    error_code ec;
    ip::address addr = ip::address::from_string(int2ipstr(itr->address.address), ec);
        if (!ec) {
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
            LIBED2K_LOG(traversal) << "traverse " << itr->kid << " " << int2ipstr(itr->address.address);
            m_algorithm->traverse(itr->kid, udp::endpoint(addr, itr->address.udp_port));
#endif
        }
    }

    done();
}

void add_entry_fun(void* userdata, node_entry const& e)
{
	traversal_algorithm* f = (traversal_algorithm*)userdata;
	f->add_entry(e.id, e.ep(), observer::flag_initial);
}

find_data::find_data(
	node_impl& node
	, node_id target
	, data_callback const& dcallback
	, nodes_callback const& ncallback
    , uint8_t search_type)
	: traversal_algorithm(node, target)
	, m_data_callback(dcallback)
	, m_nodes_callback(ncallback)
	, m_target(target)
    , m_id(node.nid())
	, m_done(false)
	, m_got_peers(false)
    , m_search_type(search_type)
{
	node.m_table.for_each_node(&add_entry_fun, 0, (traversal_algorithm*)this);
}

observer_ptr find_data::new_observer(void* ptr
	, udp::endpoint const& ep, node_id const& id)
{
	observer_ptr o(new (ptr) find_data_observer(this, ep, id));
#if defined LIBED2K_DEBUG || LIBED2K_RELEASE_ASSERTS
	o->m_in_constructor = false;
#endif
	return o;
}

bool find_data::invoke(observer_ptr o)
{
	if (m_done)
	{
		m_invoke_count = -1;
		return false;
	}

    //find peers request
    kademlia2_req req;
    req.kid_receiver = o->id();
    req.kid_target = m_target;
    req.search_type = m_search_type;
    return m_node.m_rpc.invoke(req, o->target_ep(), o);
}

void find_data::got_peers(std::vector<tcp::endpoint> const& peers)
{
	if (!peers.empty()) m_got_peers = true;
	m_data_callback(peers);
}

void find_data::done()
{
	if (m_invoke_count != 0) return;

	m_done = true;

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
	LIBED2K_LOG(traversal) << time_now_string() << "[" << this << "] get_peers DONE";
#endif

	std::vector<std::pair<node_entry, std::string> > results;
	int num_results = m_node.m_table.bucket_size();
	for (std::vector<observer_ptr>::iterator i = m_results.begin()
		, end(m_results.end()); i != end && num_results > 0; ++i)
	{
		observer_ptr const& o = *i;
		if (o->flags & observer::flag_no_id) continue;
		if ((o->flags & observer::flag_queried) == 0) continue;
		//std::map<node_id, std::string>::iterator j = m_write_tokens.find(o->id());
		//if (j == m_write_tokens.end()) continue;
        int distance = distance_exp(m_target, o->id());
//#ifdef LIBED2K_DHT_VERBOSE_LOGGING
//        LIBED2K_LOG(traversal) << o->id() << " distance " << distance;
//#endif
        //if (distance > KADEMLIA_TOLERANCE_ZONE) continue;
		results.push_back(std::make_pair(node_entry(o->id(), o->target_ep()), ""));
		--num_results;
	}

	m_nodes_callback(results, m_got_peers);

	traversal_algorithm::done();
}

} } // namespace libed2k::dht

