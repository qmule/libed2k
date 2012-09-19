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

#include <boost/bind.hpp>
#include "libed2k/config.hpp"
#include "libed2k/invariant_check.hpp"
#include "libed2k/connection_queue.hpp"
#include "libed2k/io_service.hpp"
#include "libed2k/error_code.hpp"

#if defined LIBED2K_ASIO_DEBUGGING
#include "libtorrent/debug.hpp"
#endif

namespace libed2k
{

	connection_queue::connection_queue(io_service& ios): m_next_ticket(0)
		, m_num_connecting(0)
		, m_half_open_limit(0)
		, m_abort(false)
		, m_timer(ios)
#ifdef LIBED2K_DEBUG
		, m_in_timeout_function(false)
#endif
	{
#ifdef LIBED2K_CONNECTION_LOGGING
		m_log.open("connection_queue.log");
#endif
	}

	int connection_queue::free_slots() const
	{
		mutex_t::scoped_lock l(m_mutex);
		return m_half_open_limit == 0 ? (std::numeric_limits<int>::max)()
			: m_half_open_limit - m_queue.size();
	}

	void connection_queue::enqueue(boost::function<void(int)> const& on_connect
		, boost::function<void()> const& on_timeout
		, time_duration timeout, int priority)
	{
		mutex_t::scoped_lock l(m_mutex);

		INVARIANT_CHECK;

		LIBED2K_ASSERT(priority >= 0);
		LIBED2K_ASSERT(priority < 3);

		entry* e = 0;

		switch (priority)
		{
			case 0:
				m_queue.push_back(entry());
				e = &m_queue.back();
				break;
			case 1:
			case 2:
				m_queue.push_front(entry());
				e = &m_queue.front();
				break;
		}

		e->priority = priority;
		e->on_connect = on_connect;
		e->on_timeout = on_timeout;
		e->ticket = m_next_ticket;
		e->timeout = timeout;
		++m_next_ticket;

		if (m_num_connecting < m_half_open_limit
			|| m_half_open_limit == 0)
			m_timer.get_io_service().post(boost::bind(
				&connection_queue::on_try_connect, this));
	}

	void connection_queue::done(int ticket)
	{
		mutex_t::scoped_lock l(m_mutex);

		INVARIANT_CHECK;

		std::list<entry>::iterator i = std::find_if(m_queue.begin()
			, m_queue.end(), boost::bind(&entry::ticket, _1) == ticket);
		if (i == m_queue.end())
		{
			// this might not be here in case on_timeout calls remove
			return;
		}
		if (i->connecting) --m_num_connecting;
		m_queue.erase(i);

		if (m_num_connecting < m_half_open_limit
			|| m_half_open_limit == 0)
			m_timer.get_io_service().post(boost::bind(
				&connection_queue::on_try_connect, this));
	}

	void connection_queue::close()
	{
		error_code ec;
		mutex_t::scoped_lock l(m_mutex);
		if (m_num_connecting == 0) m_timer.cancel(ec);
		m_abort = true;

		std::list<entry> tmp;
		tmp.swap(m_queue);
		m_num_connecting = 0;

		// we don't want to call the timeout callback while we're locked
		// since that is a recipie for dead-locks
		l.unlock();

		while (!tmp.empty())
		{
			entry& e = tmp.front();
			if (e.priority > 1)
			{
				mutex_t::scoped_lock ll(m_mutex);
				if (e.connecting) ++m_num_connecting;
				m_queue.push_back(e);
				tmp.pop_front();
				continue;
			}
			LIBED2K_TRY {
				e.on_timeout();
			} LIBED2K_CATCH(std::exception&) {}
			tmp.pop_front();
		}
	}

	void connection_queue::limit(int limit)
	{
		LIBED2K_ASSERT(limit >= 0);
		m_half_open_limit = limit;
	}

	int connection_queue::limit() const
	{ return m_half_open_limit; }

#ifdef LIBED2K_DEBUG

	void connection_queue::check_invariant() const
	{
		int num_connecting = 0;
		for (std::list<entry>::const_iterator i = m_queue.begin();
			i != m_queue.end(); ++i)
		{
			if (i->connecting) ++num_connecting;
			else LIBED2K_ASSERT(i->expires == max_time());
		}
		LIBED2K_ASSERT(num_connecting == m_num_connecting);
	}

#endif

	void connection_queue::try_connect(connection_queue::mutex_t::scoped_lock& l)
	{
		INVARIANT_CHECK;

#ifdef LIBED2K_CONNECTION_LOGGING
		m_log << log_time() << " " << free_slots() << std::endl;
#endif
		// if this is enabled, UPnP connections will be blocked when shutting down
//		if (m_abort) return;

		if (m_num_connecting >= m_half_open_limit
			&& m_half_open_limit > 0) return;
	
		if (m_queue.empty())
		{
			error_code ec;
			m_timer.cancel(ec);
			return;
		}

		std::list<entry>::iterator i = std::find_if(m_queue.begin()
			, m_queue.end(), boost::bind(&entry::connecting, _1) == false);

		std::list<entry> to_connect;

		while (i != m_queue.end())
		{
			LIBED2K_ASSERT(i->connecting == false);
			ptime expire = time_now_hires() + i->timeout;
			if (m_num_connecting == 0)
			{
#if defined LIBED2K_ASIO_DEBUGGING
				add_outstanding_async("connection_queue::on_timeout");
#endif
				error_code ec;
				m_timer.expires_at(expire, ec);
				m_timer.async_wait(boost::bind(&connection_queue::on_timeout, this, _1));
			}
			i->connecting = true;
			++m_num_connecting;
			i->expires = expire;

			INVARIANT_CHECK;

			to_connect.push_back(*i);

#ifdef LIBED2K_CONNECTION_LOGGING
			m_log << log_time() << " " << free_slots() << std::endl;
#endif

			if (m_num_connecting >= m_half_open_limit
				&& m_half_open_limit > 0) break;
			i = std::find_if(i, m_queue.end(), boost::bind(&entry::connecting, _1) == false);
		}

		l.unlock();

		while (!to_connect.empty())
		{
			entry& ent = to_connect.front();
			LIBED2K_TRY {
				ent.on_connect(ent.ticket);
			} LIBED2K_CATCH(std::exception&) {}
			to_connect.pop_front();
		}

	}

#ifdef LIBED2K_DEBUG
	struct function_guard
	{
		function_guard(bool& v): val(v) { LIBED2K_ASSERT(!val); val = true; }
		~function_guard() { val = false; }

		bool& val;
	};
#endif
	
	void connection_queue::on_timeout(error_code const& e)
	{
#if defined LIBED2K_ASIO_DEBUGGING
		complete_async("connection_queue::on_timeout");
#endif
		mutex_t::scoped_lock l(m_mutex);

		INVARIANT_CHECK;
#ifdef LIBED2K_DEBUG
		function_guard guard_(m_in_timeout_function);
#endif

		LIBED2K_ASSERT(!e || e == boost::asio::error::operation_aborted);
		if (e) return;

		ptime next_expire = max_time();
		ptime now = time_now_hires() + milliseconds(100);
		std::list<entry> timed_out;
		for (std::list<entry>::iterator i = m_queue.begin();
			!m_queue.empty() && i != m_queue.end();)
		{
			if (i->connecting && i->expires < now)
			{
				std::list<entry>::iterator j = i;
				++i;
				timed_out.splice(timed_out.end(), m_queue, j, i);
				--m_num_connecting;
				continue;
			}
			if (i->expires < next_expire)
				next_expire = i->expires;
			++i;
		}

		// we don't want to call the timeout callback while we're locked
		// since that is a recepie for dead-locks
		l.unlock();

		for (std::list<entry>::iterator i = timed_out.begin()
			, end(timed_out.end()); i != end; ++i)
		{
			LIBED2K_ASSERT(i->connecting);
			LIBED2K_ASSERT(i->ticket != -1);
			LIBED2K_TRY {
				i->on_timeout();
			} LIBED2K_CATCH(std::exception&) {}
		}
		
		l.lock();
		
		if (next_expire < max_time())
		{
#if defined LIBED2K_ASIO_DEBUGGING
			add_outstanding_async("connection_queue::on_timeout");
#endif
			error_code ec;
			m_timer.expires_at(next_expire, ec);
			m_timer.async_wait(boost::bind(&connection_queue::on_timeout, this, _1));
		}
		try_connect(l);
	}

	void connection_queue::on_try_connect()
	{
		mutex_t::scoped_lock l(m_mutex);
		try_connect(l);
	}
}

