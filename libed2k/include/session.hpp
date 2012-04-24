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
            resume_data(0),
            storage_mode(storage_mode_sparse),
            duplicate_is_error(false),
            seed_mode(false)
        {}

        md4_hash info_hash;
        fs::path file_path;
        std::vector<peer_entry> peer_list;
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

    private:
        void init(const fingerprint& id, const char* listen_interface,
                  const session_settings& settings);

		// data shared between the main thread
		// and the working thread
        boost::shared_ptr<aux::session_impl> m_impl;
    };
}

#endif
