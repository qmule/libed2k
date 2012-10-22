/*

Copyright (c) 2007-2011, Arvid Norberg
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

#include <libed2k/disk_buffer_pool.hpp>
#include <libed2k/assert.hpp>
#include <algorithm>

#if LIBED2K_USE_MLOCK && !defined LIBED2K_WINDOWS
#include <sys/mman.h>
#endif

#ifdef LIBED2K_DISK_STATS
#include <libed2k/time.hpp>
#endif

namespace libed2k
{
    disk_buffer_pool::disk_buffer_pool(int block_size)
        : m_block_size(block_size)
        , m_in_use(0)
#ifndef LIBED2K_DISABLE_POOL_ALLOCATOR
        , m_pool(block_size, m_settings.cache_buffer_chunk_size)
#endif
    {
#if defined LIBED2K_DISK_STATS || defined LIBED2K_STATS
        m_allocations = 0;
#endif
#ifdef LIBED2K_DISK_STATS
        m_log.open("disk_buffers.log", std::ios::trunc);
        m_categories["read cache"] = 0;
        m_categories["write cache"] = 0;

        m_disk_access_log.open("disk_access.log", std::ios::trunc);
#endif
#if defined LIBED2K_DEBUG || LIBED2K_RELEASE_ASSERTS
        m_magic = 0x1337;
#endif
    }

#if defined LIBED2K_DEBUG || LIBED2K_RELEASE_ASSERTS
    disk_buffer_pool::~disk_buffer_pool()
    {
        LIBED2K_ASSERT(m_magic == 0x1337);
        m_magic = 0;
    }
#endif

#if defined LIBED2K_DEBUG || LIBED2K_RELEASE_ASSERTS || defined LIBED2K_DISK_STATS
    bool disk_buffer_pool::is_disk_buffer(char* buffer
        , mutex::scoped_lock& l) const
    {
        LIBED2K_ASSERT(m_magic == 0x1337);
#ifdef LIBED2K_DISK_STATS
        if (m_buf_to_category.find(buffer)
            == m_buf_to_category.end()) return false;
#endif
#ifdef LIBED2K_DISABLE_POOL_ALLOCATOR
        return true;
#else
        return m_pool.is_from(buffer);
#endif
    }

    bool disk_buffer_pool::is_disk_buffer(char* buffer) const
    {
        mutex::scoped_lock l(m_pool_mutex);
        return is_disk_buffer(buffer, l);
    }
#endif

    char* disk_buffer_pool::allocate_buffer(char const* category)
    {
        mutex::scoped_lock l(m_pool_mutex);
        LIBED2K_ASSERT(m_magic == 0x1337);
#ifdef LIBED2K_DISABLE_POOL_ALLOCATOR
        char* ret = page_aligned_allocator::malloc(m_block_size);
#else
        char* ret = (char*)m_pool.malloc();
        m_pool.set_next_size(m_settings.cache_buffer_chunk_size);
#endif
        ++m_in_use;
#if LIBED2K_USE_MLOCK
        if (m_settings.lock_disk_cache)
        {
#ifdef LIBED2K_WINDOWS
            VirtualLock(ret, m_block_size);
#else
            mlock(ret, m_block_size);
#endif
        }
#endif

#if defined LIBED2K_DISK_STATS || defined LIBED2K_STATS
        ++m_allocations;
#endif
#ifdef LIBED2K_DISK_STATS
        ++m_categories[category];
        m_buf_to_category[ret] = category;
        m_log << log_time() << " " << category << ": " << m_categories[category] << "\n";
#endif
        LIBED2K_ASSERT(ret == 0 || is_disk_buffer(ret, l));
        return ret;
    }

#ifdef LIBED2K_DISK_STATS
    void disk_buffer_pool::rename_buffer(char* buf, char const* category)
    {
        mutex::scoped_lock l(m_pool_mutex);
        LIBED2K_ASSERT(is_disk_buffer(buf, l));
        LIBED2K_ASSERT(m_categories.find(m_buf_to_category[buf])
            != m_categories.end());
        std::string const& prev_category = m_buf_to_category[buf];
        --m_categories[prev_category];
        m_log << log_time() << " " << prev_category << ": " << m_categories[prev_category] << "\n";

        ++m_categories[category];
        m_buf_to_category[buf] = category;
        m_log << log_time() << " " << category << ": " << m_categories[category] << "\n";
        LIBED2K_ASSERT(m_categories.find(m_buf_to_category[buf])
            != m_categories.end());
    }
#endif

    void disk_buffer_pool::free_multiple_buffers(char** bufvec, int numbufs)
    {
        char** end = bufvec + numbufs;
        // sort the pointers in order to maximize cache hits
        std::sort(bufvec, end);

        mutex::scoped_lock l(m_pool_mutex);
        for (; bufvec != end; ++bufvec)
        {
            char* buf = *bufvec;
            LIBED2K_ASSERT(buf);
            free_buffer_impl(buf, l);;
        }
    }

    void disk_buffer_pool::free_buffer(char* buf)
    {
        mutex::scoped_lock l(m_pool_mutex);
        free_buffer_impl(buf, l);
    }

    void disk_buffer_pool::free_buffer_impl(char* buf, mutex::scoped_lock& l)
    {
        LIBED2K_ASSERT(buf);
        LIBED2K_ASSERT(m_magic == 0x1337);
        LIBED2K_ASSERT(is_disk_buffer(buf, l));
#if defined LIBED2K_DISK_STATS || defined LIBED2K_STATS
        --m_allocations;
#endif
#ifdef LIBED2K_DISK_STATS
        LIBED2K_ASSERT(m_categories.find(m_buf_to_category[buf])
            != m_categories.end());
        std::string const& category = m_buf_to_category[buf];
        --m_categories[category];
        m_log << log_time() << " " << category << ": " << m_categories[category] << "\n";
        m_buf_to_category.erase(buf);
#endif
#if LIBED2K_USE_MLOCK
        if (m_settings.lock_disk_cache)
        {
#ifdef LIBED2K_WINDOWS
            VirtualUnlock(buf, m_block_size);
#else
            munlock(buf, m_block_size);
#endif
        }
#endif
#ifdef LIBED2K_DISABLE_POOL_ALLOCATOR
        page_aligned_allocator::free(buf);
#else
        m_pool.free(buf);
#endif
        --m_in_use;
    }

    void disk_buffer_pool::release_memory()
    {
        LIBED2K_ASSERT(m_magic == 0x1337);
#ifndef LIBED2K_DISABLE_POOL_ALLOCATOR
        mutex::scoped_lock l(m_pool_mutex);
        m_pool.release_memory();
#endif
    }
}

