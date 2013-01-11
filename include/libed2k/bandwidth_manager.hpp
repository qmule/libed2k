/*

Copyright (c) 2007, Arvid Norberg
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

#ifndef LIBED2K_BANDWIDTH_MANAGER_HPP_INCLUDED
#define LIBED2K_BANDWIDTH_MANAGER_HPP_INCLUDED

#include <vector>
#include <boost/intrusive_ptr.hpp>

#include "libed2k/bandwidth_limit.hpp"
#include "libed2k/bandwidth_queue_entry.hpp"
#include "libed2k/ptime.hpp"

using boost::intrusive_ptr;

namespace libed2k {

struct LIBED2K_EXTRA_EXPORT bandwidth_manager
{
    bandwidth_manager(int channel);

    void close();

#if defined LIBED2K_DEBUG || LIBED2K_RELEASE_ASSERTS
    bool is_queued(const peer_connection* peer) const;
#endif

    int queue_size() const;
    int queued_bytes() const;
    
    // non prioritized means that, if there's a line for bandwidth,
    // others will cut in front of the non-prioritized peers.
    // this is used by web seeds
    // returns the number of bytes to assign to the peer, or 0
    // if the peer's 'assign_bandwidth' callback will be called later
    int request_bandwidth(const intrusive_ptr<peer_connection>& peer
        , int blk, int priority
        , bandwidth_channel* chan1 = 0
        , bandwidth_channel* chan2 = 0
        , bandwidth_channel* chan3 = 0
        , bandwidth_channel* chan4 = 0
        , bandwidth_channel* chan5 = 0);

#ifdef LIBED2K_DEBUG
    void check_invariant() const;
#endif

    void update_quotas(time_duration const& dt);

    // these are the consumers that want bandwidth
    typedef std::vector<bw_request> queue_t;
    queue_t m_queue;
    // the number of bytes all the requests in queue are for
    int m_queued_bytes;

    // this is the channel within the consumers
    // that bandwidth is assigned to (upload or download)
    int m_channel;

    bool m_abort;
};

}

#endif

