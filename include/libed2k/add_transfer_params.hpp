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

        add_transfer_params(
            const md4_hash& hash, size_t nSize, const std::string& filename,
            const bitfield& ps, const std::vector<md4_hash>& hset)
        {
            reset();
            m_filename = filename;
            file_size = nSize;
            piece_hashses = hset;
            seed_mode = true;
        }

        md4_hash file_hash;
        std::string m_filename; // full filename in UTF8 always!
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
                    m_filename == t.m_filename &&
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
