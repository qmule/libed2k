#include "libed2k/add_transfer_params.hpp"
#include "libed2k/log.hpp"

namespace libed2k
{

    void add_transfer_params::dump() const
        {
            DBG("add_transfer_params::dump");
            DBG("file hash: " << file_hash << " all hashes size: " << hashset.size());
            DBG("file full name: " << convert_to_native(m_filename));
            DBG("file size: " << file_size);
            DBG("accepted: " << accepted <<
                " requested: " << requested <<
                " transf: " << transferred <<
                " priority: " << priority);
        }

        void add_transfer_params::reset()
        {
            file_size = 0;
            seed_mode = false;
            num_complete_sources = -1;
            num_incomplete_sources = -1;
            resume_data = 0;
            storage_mode = storage_mode_sparse;
            duplicate_is_error = false;
            accepted = 0;
            requested = 0;
            transferred = 0;
            priority = 0;
        }
}
