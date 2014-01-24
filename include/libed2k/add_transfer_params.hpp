#ifndef __ADD_TRANSFER_PARAMS_HPP__
#define __ADD_TRANSFER_PARAMS_HPP__

#include "libed2k/md4_hash.hpp"
#include "libed2k/storage_defs.hpp"
#include "libed2k/peer.hpp"
#include "libed2k/bitfield.hpp"

namespace libed2k
{
    class add_transfer_params
    {
    public:
        add_transfer_params() { reset(); }

        /**
          * @param filepath - UTF8
         */
        add_transfer_params(
            const md4_hash& hash, size_t nSize, const std::string& filepath,
            const bitfield& ps, const std::vector<md4_hash>& hset)
        {
            reset();
            file_path = filepath;
            file_size = nSize;
            piece_hashses = hset;
            seed_mode = true;
        }

        add_transfer_params(const std::string& filepath)
        {
            reset();
            file_path = filepath;
        }


        md4_hash file_hash;
        std::string file_path; // full filename in UTF8 always!
        size_type  file_size;
        std::vector<md4_hash> piece_hashses;
        std::vector<char>* resume_data;
        storage_mode_t storage_mode;
        bool duplicate_is_error;
        bool seed_mode;
        int num_complete_sources;
        int num_incomplete_sources;

        boost::uint32_t accepted;
        boost::uint32_t requested;
        boost::uint64_t transferred;
        boost::uint8_t  priority;

        bool operator==(const add_transfer_params& t) const
        {
            return (file_hash == t.file_hash &&
                    file_path == t.file_path &&
                    file_size == t.file_size &&
                    piece_hashses == t.piece_hashses &&
                    accepted == t.accepted &&
                    requested == t.requested &&
                    transferred == t.transferred &&
                    priority  == t.priority);
        }

        void dump() const;

    private:
        void reset();
    };
}

#endif //__ADD_TRANSFER_PARAMS_HPP__
