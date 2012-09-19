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

#ifndef LIBED2K_DISK_BUFFER_POOL
#define LIBED2K_DISK_BUFFER_POOL

#include <libed2k/config.hpp>
#include <libed2k/thread.hpp>
#include <libed2k/socket.hpp>
#include <libed2k/session_settings.hpp>
#include <libed2k/allocator.hpp>

#ifndef LIBED2K_DISABLE_POOL_ALLOCATOR
#include <boost/pool/pool.hpp>
#endif

#ifdef LIBED2K_DISK_STATS
#include <fstream>
#endif

#if defined LIBED2K_DEBUG || LIBED2K_RELEASE_ASSERTS
#include <map>
#endif

namespace libed2k
{
    struct LIBED2K_EXTRA_EXPORT disk_buffer_pool : boost::noncopyable
    {
        disk_buffer_pool(int block_size);
#if defined LIBED2K_DEBUG || LIBED2K_RELEASE_ASSERTS
        ~disk_buffer_pool();
#endif

#if defined LIBED2K_DEBUG || LIBED2K_RELEASE_ASSERTS
        bool is_disk_buffer(char* buffer
            , mutex::scoped_lock& l) const;
        bool is_disk_buffer(char* buffer) const;
#endif

        char* allocate_buffer(char const* category);
        void free_buffer(char* buf);
        void free_multiple_buffers(char** bufvec, int numbufs);

        int block_size() const { return m_block_size; }

#ifdef LIBED2K_STATS
        int disk_allocations() const
        { return m_allocations; }
#endif

#ifdef LIBED2K_DISK_STATS
        std::ofstream m_disk_access_log;
#endif

        void release_memory();

        int in_use() const { return m_in_use; }

    protected:

        void free_buffer_impl(char* buf, mutex::scoped_lock& l);

        // number of bytes per block. The ED2K
        // protocol defines the block size to BLOCK_SIZE.
        const int m_block_size;

        // number of disk buffers currently allocated
        int m_in_use;

        session_settings m_settings;

    private:

        mutable mutex m_pool_mutex;

#ifndef LIBED2K_DISABLE_POOL_ALLOCATOR
        // memory pool for read and write operations
        // and disk cache
        boost::pool<page_aligned_allocator> m_pool;
#endif

#if defined LIBED2K_DISK_STATS || defined LIBED2K_STATS
        int m_allocations;
#endif
#ifdef LIBED2K_DISK_STATS
    public:
        void rename_buffer(char* buf, char const* category);
    protected:
        std::map<std::string, int> m_categories;
        std::map<char*, std::string> m_buf_to_category;
        std::ofstream m_log;
    private:
#endif
#if defined LIBED2K_DEBUG || LIBED2K_RELEASE_ASSERTS
        int m_magic;
#endif
    };

}

#endif // LIBED2K_DISK_BUFFER_POOL

