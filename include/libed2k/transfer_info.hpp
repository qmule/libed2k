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

#ifndef LIBED2K_TRANSFER_INFO_HPP_INCLUDED
#define LIBED2K_TRANSFER_INFO_HPP_INCLUDED

#include <string>
#include <vector>

#include "libed2k/config.hpp"
#include "libed2k/size_type.hpp"
#include "libed2k/intrusive_ptr_base.hpp"
#include "libed2k/assert.hpp"
#include "libed2k/file_storage.hpp"
#include "libed2k/copy_ptr.hpp"
#include "libed2k/md4_hash.hpp"
#include "libed2k/constants.hpp"

namespace libed2k
{
    class LIBED2K_EXPORT transfer_info : public intrusive_ptr_base<transfer_info>
    {
    public:

#ifdef LIBED2K_DEBUG
        void check_invariant() const;
#endif

        transfer_info(transfer_info const& t);
        transfer_info(md4_hash const& info_hash, const std::string& filename,
                      size_type filesize, const std::vector<md4_hash>& piece_hashses = std::vector<md4_hash>());

        ~transfer_info();

        file_storage const& files() const { return m_files; }
        file_storage const& orig_files() const { return m_orig_files ? *m_orig_files : m_files; }

        void rename_file(int index, std::string const& new_filename)
        {
            copy_on_write();
            m_files.rename_file(index, new_filename);
        }

#if LIBED2K_USE_WSTRING
        void rename_file(int index, std::wstring const& new_filename)
        {
            copy_on_write();
            m_files.rename_file(index, new_filename);
        }
#endif // LIBED2K_USE_WSTRING

        void remap_files(file_storage const& f);

        size_type total_size() const { return m_files.total_size(); }
        int piece_length() const { return m_files.piece_length(); }
        int num_pieces() const { return m_files.num_pieces(); }
        const md4_hash& info_hash() const { return m_info_hash; }
        const std::string& name() const { return m_files.name(); }

        typedef file_storage::iterator file_iterator;
        typedef file_storage::reverse_iterator reverse_file_iterator;

        file_iterator begin_files() const { return m_files.begin(); }
        file_iterator end_files() const { return m_files.end(); }
        reverse_file_iterator rbegin_files() const { return m_files.rbegin(); }
        reverse_file_iterator rend_files() const { return m_files.rend(); }
        int num_files() const { return m_files.num_files(); }
        file_entry file_at(int index) const { return m_files.at(index); }

        file_iterator file_at_offset(size_type offset) const
        { return m_files.file_at_offset(offset); }
        std::vector<file_slice> map_block(int piece, size_type offset, int size) const
        { return m_files.map_block(piece, offset, size); }
        peer_request map_file(int file, size_type offset, int size) const
        { return m_files.map_file(file, offset, size); }

        bool is_valid() const
        {
            return m_files.is_valid() &&
                int(m_piece_hashes.size()) == num_pieces() + (file_at(0).size % PIECE_SIZE == 0);
        }

        int piece_size(int index) const { return m_files.piece_size(index); }
        const std::vector<md4_hash>& piece_hashses() const { return m_piece_hashes; }
        const md4_hash& hash_for_piece(int index) const { return m_piece_hashes.at(index); }

        void piece_hashses(const std::vector<md4_hash>& hs) { m_piece_hashes = hs; }
        void swap(transfer_info& ti);

        // if we're logging member offsets, we need access to them
#if defined LIBED2K_DEBUG \
        && !defined LIBED2K_LOGGING \
        && !defined LIBED2K_VERBOSE_LOGGING \
        && !defined LIBED2K_ERROR_LOGGING
    private:
#endif

        // not assignable
        transfer_info const& operator=(transfer_info const&);

        void copy_on_write();

        file_storage m_files;

        // if m_files is modified, it is first copied into
        // m_orig_files so that the original name and
        // filenames are preserved.
        copy_ptr<const file_storage> m_orig_files;

        // the hash that identifies this transfer
        md4_hash m_info_hash;
        // hashes of the pieces
        std::vector<md4_hash> m_piece_hashes;
    };
}

#endif // LIBED2K_TRANSFER_INFO_HPP_INCLUDED
