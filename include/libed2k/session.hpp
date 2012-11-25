#ifndef __LIBED2K_SESSION__
#define __LIBED2K_SESSION__

#include <string>
#include <boost/shared_ptr.hpp>

#include "libed2k/storage_defs.hpp"
#include "libed2k/fingerprint.hpp"
#include "libed2k/md4_hash.hpp"
#include "libed2k/transfer_handle.hpp"
#include "libed2k/peer_connection_handle.hpp"
#include "libed2k/peer.hpp"
#include "libed2k/alert.hpp"
#include "libed2k/packet_struct.hpp"
#include "libed2k/session_status.hpp"
#include "libed2k/filesystem.hpp"

namespace libed2k {

    class session_settings;
    struct transfer_handle;
    class add_transfer_params;

    namespace aux
    {
        class session_impl;
    }


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

        session_status status() const;

        // all transfer_handles must be destructed before the session is destructed!
        transfer_handle add_transfer(const add_transfer_params& params);
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
        bool listen_on(int port, const char* net_interface = 0);
        bool is_listening() const;
        unsigned short listen_port() const;
        void set_settings(const session_settings& settings);
        session_settings settings() const;

        /**
          * search sources for file
         */
        void post_sources_request(const md4_hash& hFile, boost::uint64_t nSize);

        int download_rate_limit() const;
        int upload_rate_limit() const;

        void server_conn_start();
        void server_conn_stop();
        bool server_conn_online() const;
        bool server_conn_offline() const;

        void pause();
        void resume();
        void make_transfer_parameters(const std::string& filepath);
        void cancel_transfer_parameters(const std::string& filepath);

    private:
        void init(const fingerprint& id, const char* listen_interface,
                  const session_settings& settings);

		// data shared between the main thread
		// and the working thread
        boost::shared_ptr<aux::session_impl> m_impl;
    };
}

#endif
