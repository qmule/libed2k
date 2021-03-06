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

#ifndef LIBED2K_CONNECTION_QUEUE
#define LIBED2K_CONNECTION_QUEUE

#include <list>
#include <boost/function/function1.hpp>
#include <boost/function/function0.hpp>
#include <boost/noncopyable.hpp>
#include "libed2k/io_service.hpp"
#include "libed2k/error_code.hpp"
#include "libed2k/deadline_timer.hpp"
#include "libed2k/ptime.hpp"

#ifdef LIBED2K_CONNECTION_LOGGING
#include <fstream>
#endif

#include "libed2k/thread.hpp"

namespace libed2k
{

class LIBED2K_EXTRA_EXPORT connection_queue : public boost::noncopyable
{
public:
	connection_queue(io_service& ios);

	// if there are no free slots, returns the negative
	// number of queued up connections
	int free_slots() const;

	void enqueue(boost::function<void(int)> const& on_connect
		, boost::function<void()> const& on_timeout
		, time_duration timeout, int priority = 0);
	void done(int ticket);
	void limit(int limit);
	int limit() const;
	void close();
	int size() const { return m_queue.size(); }

#ifdef LIBED2K_DEBUG
	void check_invariant() const;
#endif

private:

	typedef mutex mutex_t;

	void try_connect(mutex_t::scoped_lock& l);
	void on_timeout(error_code const& e);
	void on_try_connect();

	struct entry
	{
		entry(): connecting(false), ticket(0), expires(max_time()), priority(0) {}
		// called when the connection is initiated
		// this is when the timeout countdown starts
		boost::function<void(int)> on_connect;
		// called if done hasn't been called within the timeout
		// or if the connection queue aborts. This means there
		// are 3 different interleaves of these function calls:
		// 1. on_connect
		// 2. on_connect, on_timeout
		// 3. on_timeout
		boost::function<void()> on_timeout;
		bool connecting;
		int ticket;
		ptime expires;
		time_duration timeout;
		int priority;
	};

	std::list<entry> m_queue;

	// the next ticket id a connection will be given
	int m_next_ticket;
	int m_num_connecting;
	int m_half_open_limit;
	bool m_abort;

	deadline_timer m_timer;

	mutable mutex_t m_mutex;

#ifdef LIBED2K_DEBUG
	bool m_in_timeout_function;
#endif
#ifdef LIBED2K_CONNECTION_LOGGING
	std::ofstream m_log;
#endif
};

}

#endif

