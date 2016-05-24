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

#include <libed2k/kademlia/refresh.hpp>
#include <libed2k/kademlia/rpc_manager.hpp>
#include <libed2k/kademlia/node.hpp>

#include <libed2k/io.hpp>

namespace libed2k { namespace dht
{

#ifdef LIBED2K_DHT_VERBOSE_LOGGING
	LIBED2K_DECLARE_LOG(traversal);
#endif

refresh::refresh(
	node_impl& node
	, node_id target
	, done_callback const& callback)
	: find_data(node, target, find_data::data_callback(), callback, 0)
{
}

char const* refresh::name() const
{
	return "refresh";
}

observer_ptr refresh::new_observer(void* ptr
	, udp::endpoint const& ep, node_id const& id)
{
	observer_ptr o(new (ptr) find_data_observer(this, ep, id));
#if defined LIBED2K_DEBUG || LIBED2K_RELEASE_ASSERTS
	o->m_in_constructor = false;
#endif
	return o;
}

bool refresh::invoke(observer_ptr o)
{
    kad2_hello_req p;
    return m_node.m_rpc.invoke(p, o->target_ep(), o);
}

bootstrap::bootstrap(
	node_impl& node
	, node_id target
	, done_callback const& callback)
	: refresh(node, target, callback)
{
	// make it more resilient to nodes not responding.
	// we don't want to terminate early when we're bootstrapping
	m_num_target_nodes *= 2;
}

char const* bootstrap::name() const { return "bootstrap"; }

void bootstrap::done()
{
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
	LIBED2K_LOG(traversal) << " [" << this << "]"
		<< " bootstrap done, pinging remaining nodes";
#endif

	for (std::vector<observer_ptr>::iterator i = m_results.begin()
		, end(m_results.end()); i != end; ++i)
	{
		if ((*i)->flags & observer::flag_queried) continue;
		// this will send a ping
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
        LIBED2K_LOG(traversal) << "ping " << int2ipstr(address2int((*i)->target_ep().address()));
#endif
		m_node.add_node((*i)->target_ep(), (*i)->id());
	}
	refresh::done();
}

bool bootstrap::invoke(observer_ptr o) {
#ifdef LIBED2K_DHT_VERBOSE_LOGGING
    LIBED2K_LOG(traversal) << " [" << this << "]"
        << " bootstrapping ==> " << o->target_addr();
#endif
    kad2_bootstrap_req p;
    return m_node.m_rpc.invoke(p, o->target_ep(), o);
}

} } // namespace libed2k::dht

