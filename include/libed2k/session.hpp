#ifndef __LIBED2K_SESSION__
#define __LIBED2K_SESSION__

#include <string>
#include <boost/shared_ptr.hpp>

#include "libed2k/storage_defs.hpp"
#include "libed2k/fingerprint.hpp"
#include "libed2k/hasher.hpp"
#include "libed2k/transfer_handle.hpp"
#include "libed2k/peer_connection_handle.hpp"
#include "libed2k/peer.hpp"
#include "libed2k/alert.hpp"
#include "libed2k/packet_struct.hpp"
#include "libed2k/kademlia/kad_packet_struct.hpp"
#include "libed2k/session_status.hpp"
#include "libed2k/filesystem.hpp"
#include "libed2k/session_settings.hpp"
#include "libed2k/entry.hpp"

namespace libed2k {

    class session_settings;
    struct transfer_handle;
    class add_transfer_params;
    struct ip_filter;
    class upnp;
    class natpmp;
    struct server_connection_parameters;

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
        void post_transfer(const add_transfer_params& params);
        transfer_handle find_transfer(const md4_hash& hash) const;
        std::vector<transfer_handle> get_transfers() const;
        std::vector<transfer_handle> get_active_transfers() const;
        void remove_transfer(const transfer_handle& h, int options = none);

        peer_connection_handle add_peer_connection(const net_identifier& np);
        peer_connection_handle find_peer_connection(const net_identifier& np) const;
        peer_connection_handle find_peer_connection(const md4_hash& hash) const;

        std::auto_ptr<alert> pop_alert();
        size_t set_alert_queue_size_limit(size_t queue_size_limit_);
        void set_alert_mask(boost::uint32_t m);
        alert const* wait_for_alert(time_duration max_wait);
        void set_alert_dispatch(boost::function<void(alert const&)> const& fun);

        /** execute search file on server */
        void post_search_request(search_request& sr);
        void post_search_more_result_request();
        void post_cancel_search();
        void listen_on(int port, const char* net_interface = 0);
        bool is_listening() const;
        unsigned short listen_port() const;
        void set_settings(const session_settings& settings);
        session_settings settings() const;

        void set_ip_filter(const ip_filter& f);
        const ip_filter& get_ip_filter() const;

        /** search sources for file */
        void post_sources_request(const md4_hash& hFile, boost::uint64_t nSize);

        int download_rate_limit() const;
        int upload_rate_limit() const;

        void server_connect(const server_connection_parameters&);
        void server_disconnect();
        bool server_connection_established() const;

        void pause();
        void resume();
        void make_transfer_parameters(const std::string& filepath);
        void cancel_transfer_parameters(const std::string& filepath);

        // protocols used by add_port_mapping()
        enum protocol_type { udp = 1, tcp = 2 };
        void start_natpmp();
        void start_upnp();

        void stop_natpmp();
        void stop_upnp();

#ifndef LIBED2K_DISABLE_DHT
        void start_dht(entry const& startup_state = entry());
        void stop_dht();
        void set_dht_settings(dht_settings const& settings);
        void add_dht_node(std::pair<std::string, int> const& node);
        void add_dht_node(std::pair<std::string, int> const& node, const std::string& id);
        void add_dht_router(std::pair<std::string, int> const& node);
        bool is_dht_running() const;
        void find_keyword(const std::string& keyword);
        entry dht_state();
        kad_state dht_estate();
#endif

        // add_port_mapping adds a port forwarding on UPnP and/or NAT-PMP,
        // whichever is enabled. The return value is a handle referring to the
        // port mapping that was just created. Pass it to delete_port_mapping()
        // to remove it.
        int add_port_mapping(protocol_type t, int external_port, int local_port);
        void delete_port_mapping(int handle);


    private:
        void init(const fingerprint& id, const char* listen_interface,
                  const session_settings& settings);

		// data shared between the main thread
		// and the working thread
        boost::shared_ptr<aux::session_impl> m_impl;
    };
}

#endif
