/*

Copyright (c) 2003-2008, Arvid Norberg
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


#include <libed2k/config.hpp>
#include <libed2k/transfer_info.hpp>
#include <libed2k/filesystem.hpp>
#include <libtorrent/invariant_check.hpp>
#include <libed2k/util.hpp>

namespace libed2k
{
    transfer_info::transfer_info(transfer_info const& t)
        : m_files(t.m_files)
        , m_orig_files(t.m_orig_files)
        , m_info_hash(t.m_info_hash)
        , m_piece_hashes(t.m_piece_hashes)
    {
#if defined LIBED2K_DEBUG && !defined LIBED2K_DISABLE_INVARIANT_CHECKS
        t.check_invariant();
#endif
        INVARIANT_CHECK;
    }

    void transfer_info::remap_files(file_storage const& f)
    {
        INVARIANT_CHECK;

        // the new specified file storage must have the exact
        // same size as the current file storage
        LIBED2K_ASSERT(m_files.total_size() == f.total_size());

        if (m_files.total_size() != f.total_size()) return;
        copy_on_write();
        m_files = f;
        m_files.set_num_pieces(m_orig_files->num_pieces());
        m_files.set_piece_length(m_orig_files->piece_length());
    }

    // constructor used for creating new transfers
    // will not contain any hashes
    // just the necessary to use it with piece manager
    // used for transfers with no metadata
    transfer_info::transfer_info(
        md4_hash const& info_hash, const std::string& filename,
        size_type filesize, const std::vector<md4_hash>& piece_hashses)
        : m_info_hash(info_hash), m_piece_hashes(piece_hashses)
    {
        m_files.set_num_pieces(div_ceil(filesize, PIECE_SIZE));
        m_files.set_piece_length(PIECE_SIZE);
        m_files.add_file(filename, filesize);

        if (m_piece_hashes.empty() && filesize < PIECE_SIZE)
            m_piece_hashes.push_back(info_hash);
    }

    transfer_info::~transfer_info()
    {}

    void transfer_info::copy_on_write()
    {
        INVARIANT_CHECK;

        if (m_orig_files) return;
        m_orig_files.reset(new file_storage(m_files));
    }

    void transfer_info::swap(transfer_info& ti)
    {
        INVARIANT_CHECK;

        using std::swap;
        m_files.swap(ti.m_files);
        m_orig_files.swap(ti.m_orig_files);
        swap(m_info_hash, ti.m_info_hash);
        swap(m_piece_hashes, ti.m_piece_hashes);
    }

#ifdef LIBED2K_DEBUG
    void transfer_info::check_invariant() const
    {
        for (file_storage::iterator i = m_files.begin()
            , end(m_files.end()); i != end; ++i)
        {
            LIBED2K_ASSERT(i->name != 0);
        }
    }
#endif

}
