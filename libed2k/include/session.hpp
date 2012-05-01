// ed2k session

#ifndef __LIBED2K_SESSION__
#define __LIBED2K_SESSION__

#include <string>
#include <boost/shared_ptr.hpp>

#include <libtorrent/storage.hpp>

#include "fingerprint.hpp"
#include "md4_hash.hpp"
#include "transfer_handle.hpp"
#include "peer.hpp"
#include "alert.hpp"
#include "packet_struct.hpp"

namespace libed2k {

    typedef libtorrent::storage_constructor_type storage_constructor_type;

    class session_settings;
    namespace aux {
        class session_impl;
    }

    enum storage_mode_t
    {
        storage_mode_allocate = 0,
        storage_mode_sparse,
        storage_mode_compact
    };

    class add_transfer_params
    {
    public:
        add_transfer_params():
            file_size(0),
            resume_data(0),
            storage_mode(storage_mode_sparse),
            duplicate_is_error(false),
            seed_mode(false)
        {}

        md4_hash file_hash;
        fs::path file_path;
        size_t file_size;
        std::vector<md4_hash> hash_set;
        std::vector<char>* resume_data;
        storage_mode_t storage_mode;
        bool duplicate_is_error;
        bool seed_mode;
    };


    // Once it's created, the session object will spawn the main thread
    // that will do all the work. The main thread will be idle as long 
    // it doesn't have any transfers to participate in.
    // TODO: should inherit the session_base interfase in future
    class session
    {
    public:
        session(const fingerprint& id, const char* listen_interface,
                const session_settings& settings)
        {
            init(id, listen_interface, settings);
        }
        ~session();

        // all transfer_handles must be destructed before the session is destructed!
        transfer_handle add_transfer(const add_transfer_params& params);
        std::vector<transfer_handle> add_transfer_dir(const fs::path& dir);

        std::auto_ptr<alert> pop_alert();
        size_t set_alert_queue_size_limit(size_t queue_size_limit_);
        void set_alert_mask(boost::uint32_t m);
        alert const* wait_for_alert(time_duration max_wait);
        void set_alert_dispatch(boost::function<void(alert const&)> const& fun);

        /**
          * execute search file on server
         */
        void post_search_request(search_request& sr);

        /**
          * search sources for file
         */
        void post_sources_request(const md4_hash& hFile, boost::uint64_t nSize);
    private:
        void init(const fingerprint& id, const char* listen_interface,
                  const session_settings& settings);

		// data shared between the main thread
		// and the working thread
        boost::shared_ptr<aux::session_impl> m_impl;
    };
}

#endif
