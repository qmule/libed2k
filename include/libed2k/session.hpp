// ed2k session

#ifndef __LIBED2K_SESSION__
#define __LIBED2K_SESSION__

#include <string>
#include <boost/shared_ptr.hpp>

#include <libtorrent/storage.hpp>

#include "libed2k/fingerprint.hpp"
#include "libed2k/md4_hash.hpp"
#include "libed2k/transfer_handle.hpp"
#include "libed2k/peer_connection_handle.hpp"
#include "libed2k/peer.hpp"
#include "libed2k/alert.hpp"
#include "libed2k/packet_struct.hpp"

namespace libed2k {

    typedef libtorrent::storage_constructor_type storage_constructor_type;

    class session_settings;
    struct transfer_handle;
    class rule;
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
        add_transfer_params() { reset(); }

        add_transfer_params(const md4_hash& hash, size_t nSize, const fs::path& cpath, const fs::path& fpath,
                            const bitfield& ps, const std::vector<md4_hash>& hset)
        {
            reset();
            file_hash = hash;
            file_path = fpath;
            collection_path = cpath;
            file_size = nSize;
            pieces = ps;
            hashset = hset;
            seed_mode = true;
        }

        md4_hash file_hash;
        fs::path file_path; // in UTF8 always!
        fs::path collection_path;
        fsize_t  file_size;
        bitfield pieces;
        std::vector<md4_hash> hashset;
        std::vector<peer_entry> peer_list;
        std::vector<char>* resume_data;
        storage_mode_t storage_mode;
        bool duplicate_is_error;
        bool seed_mode;

        boost::uint32_t accepted;
        boost::uint32_t requested;
        boost::uint64_t transferred;
        boost::uint8_t  priority;

        bool operator==(const add_transfer_params& t) const
        {
            return (file_hash == t.file_hash &&
                    file_path == t.file_path &&
                    file_size == t.file_size &&
                    //pieces == t.pieces &&
                    hashset == t.hashset &&
                    accepted == t.accepted &&
                    requested == t.requested &&
                    transferred == t.transferred &&
                    priority  == t.priority &&
                    collection_path == t.collection_path
                    );
        }

        void dump() const;

    private:
        void reset();
    };

    typedef boost::function<void (const add_transfer_params&)> add_transfer_handler;


    // Once it's created, the session object will spawn the main thread
    // that will do all the work. The main thread will be idle as long 
    // it doesn't have any transfers to participate in.
    // TODO: should inherit the session_base interfase in future
    class session
    {
    public:
        enum options_t
        {
            none = 0,
            delete_files = 1
        };

        session(const fingerprint& id, const char* listen_interface,
                const session_settings& settings)
        {
            init(id, listen_interface, settings);
        }
        ~session();

        // all transfer_handles must be destructed before the session is destructed!
        transfer_handle add_transfer(const add_transfer_params& params);
        std::vector<transfer_handle> add_transfer_dir(const fs::path& dir);
        transfer_handle find_transfer(const md4_hash& hash) const;
        std::vector<transfer_handle> get_transfers() const;
        void remove_transfer(const transfer_handle& h, int options = none);

        peer_connection_handle add_peer_connection(const net_identifier& np);
        peer_connection_handle find_peer_connection(const net_identifier& np) const;
        peer_connection_handle find_peer_connection(const md4_hash& hash) const;

        std::auto_ptr<alert> pop_alert();
        size_t set_alert_queue_size_limit(size_t queue_size_limit_);
        void set_alert_mask(boost::uint32_t m);
        alert const* wait_for_alert(time_duration max_wait);
        void set_alert_dispatch(boost::function<void(alert const&)> const& fun);

        /**
          * execute search file on server
         */
        void post_search_request(search_request& sr);
        void post_search_more_result_request();
        void post_cancel_search();

        /**
          * search sources for file
         */
        void post_sources_request(const md4_hash& hFile, boost::uint64_t nSize);

        int download_rate_limit() const;
        int upload_rate_limit() const;

        void server_conn_start();
        void server_conn_stop();

        void pause();
        void resume();

        /**
          * share single file, collection is empty
         */
        void share_file(const std::string& strFilename);
        void unshare_file(const std::string& strFilename);

        /**
          * share directory, when root < path - collection name will generate
         */
        void share_dir(const std::string& strRoot, const std::string& strPath, const std::deque<std::string>& excludes);
        void unshare_dir(const std::string& strRoot, const std::string& strPath, const std::deque<std::string>& excludes);
    private:
        void init(const fingerprint& id, const char* listen_interface,
                  const session_settings& settings);

		// data shared between the main thread
		// and the working thread
        boost::shared_ptr<aux::session_impl> m_impl;
    };
}

#endif
