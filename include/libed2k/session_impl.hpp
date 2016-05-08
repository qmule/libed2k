
#ifndef __LIBED2K_SESSION_IMPL__
#define __LIBED2K_SESSION_IMPL__

#include <string>
#include <map>
#include <set>

#include <boost/pool/object_pool.hpp>

#include "libed2k/socket.hpp"
#include "libed2k/stat.hpp"
#include "libed2k/fingerprint.hpp"
#include "libed2k/hasher.hpp"
#include "libed2k/transfer_handle.hpp"
#include "libed2k/peer_connection_handle.hpp"
#include "libed2k/ip_filter.hpp"
#include "libed2k/session_settings.hpp"
#include "libed2k/util.hpp"
#include "libed2k/alert.hpp"
#include "libed2k/packet_struct.hpp"
#include "libed2k/file.hpp"
#include "libed2k/disk_io_thread.hpp"
#include "libed2k/file_pool.hpp"
#include "libed2k/bandwidth_manager.hpp"
#include "libed2k/connection_queue.hpp"
#include "libed2k/session_status.hpp"
#include "libed2k/io_service.hpp"
#include "libed2k/udp_socket.hpp"
#include "libed2k/bloom_filter.hpp"
#include "libed2k/kademlia/dht_tracker.hpp"

#ifdef LIBED2K_UPNP_LOGGING
#include <fstream>
#endif

namespace libed2k {

    class peer_connection;
    class server_connection;
    class transfer;
    class add_transfer_params;
    struct transfer_handle;
    struct listen_socket_t;
    class upnp;
    class natpmp;

    namespace aux
    {
        struct listen_socket_t
        {
            listen_socket_t(): external_port(0), ssl(false) {}

            // this is typically empty but can be set
            // to the WAN IP address of NAT-PMP or UPnP router
            address external_address;

            // this is typically set to the same as the local
            // listen port. In case a NAT port forward was
            // successfully opened, this will be set to the
            // port that is open on the external (NAT) interface
            // on the NAT box itself. This is the port that has
            // to be published to peers, since this is the port
            // the client is reachable through.
            int external_port;

            // set to true if this is an SSL listen socket
            bool ssl;

            // the actual socket
            boost::shared_ptr<socket_acceptor> sock;
        };

        // used to initialize the g_current_time before
        // anything else
        struct initialize_timer
        {
            initialize_timer();
        };

        /** class used for testing */
        class session_impl_base : boost::noncopyable, initialize_timer
        {
        public:
            typedef std::map<std::pair<std::string, boost::uint32_t>,   md4_hash> transfer_filename_map;
            typedef std::map<md4_hash, boost::shared_ptr<transfer> >    transfer_map;
            typedef std::map<client_id_type, md4_hash>                  lowid_callbacks_map;

            session_impl_base(const session_settings& settings);
            virtual ~session_impl_base();

            virtual void abort();

            /**  add transfer from another thread */
            virtual void post_transfer(add_transfer_params const& );
            virtual transfer_handle add_transfer(add_transfer_params const&, error_code& ec) = 0;
            virtual boost::weak_ptr<transfer> find_transfer(const std::string& filename) = 0;
            virtual void remove_transfer(const transfer_handle& h, int options) = 0;
            virtual std::vector<transfer_handle> get_transfers() = 0;

            /** alerts */
            std::auto_ptr<alert> pop_alert();
            void set_alert_mask(boost::uint32_t m);
            size_t set_alert_queue_size_limit(size_t queue_size_limit_);
            void set_alert_dispatch(boost::function<void(alert const&)> const&);
            alert const* wait_for_alert(time_duration max_wait);
            md4_hash callbacked_lowid(client_id_type);
            bool register_callback(client_id_type, md4_hash);
            void cleanup_callbacks();

            // this is where all active sockets are stored.
            // the selector can sleep while there's no activity on
            // them
            mutable io_service m_io_service;

            // set to true when the session object
            // is being destructed and the thread
            // should exit
            bool m_abort;

            // the settings for the client
            session_settings m_settings;
            // all transfers in the session
            transfer_map m_transfers;
            // active transfers in the session
            transfer_map m_active_transfers;

            typedef std::list<boost::shared_ptr<transfer> > check_queue_t;

            // this has all torrents that wants to be checked in it
            check_queue_t m_queued_for_checking;

            // statistics gathered from all transfers.
            stat m_stat;

            // handles delayed alerts
            alert_manager m_alerts;

            /** file hasher closed in self thread */
            transfer_params_maker   m_tpm;
            lowid_callbacks_map     lowid_conn_dict;
        };

        class session_impl : public session_impl_base
        {
        private:
#ifdef LIBED2K_UPNP_LOGGING
            std::ofstream m_upnp_log;
#endif
        public:

            // the size of each allocation that is chained in the send buffer
            enum { send_buffer_size = 128 };
            typedef std::set<boost::intrusive_ptr<peer_connection> > connection_map;

            session_impl(const fingerprint& id, const char* listen_interface,
                         const session_settings& settings);
            ~session_impl();

            void set_settings(const session_settings& s);
            const session_settings& settings() const;

            // main thread entry point
            void operator()();

            void open_listen_port();

            void set_ip_filter(const ip_filter& f);
            const ip_filter& get_ip_filter() const;

            bool listen_on(int port, const char* net_interface);
            bool is_listening() const;
            boost::uint16_t listen_port() const;
            boost::uint16_t ssl_listen_port() const;

            void update_disk_thread_settings();

            void async_accept(boost::shared_ptr<tcp::acceptor> const& listener);
            void on_accept_connection(boost::shared_ptr<tcp::socket> const& s,
                                      boost::weak_ptr<tcp::acceptor> listener,
                                      error_code const& e);

            void incoming_connection(boost::shared_ptr<tcp::socket> const& s);

            void on_port_map_log(char const* msg, int map_transport);

            // called when a port mapping is successful, or a router returns
            // a failure to map a port
            void on_port_mapping(int mapping, address const& ip, int port, error_code const& ec, int nat_transport);

			void on_receive_udp(error_code const& e, udp::endpoint const& ep, char const* buf, int len);
			void on_receive_udp_hostname(error_code const& e, char const* hostname, char const* buf, int len);

            void maybe_update_udp_mapping(int nat, int local_port, int external_port);

            enum
            {
                source_dht = 1,
                source_peer = 2,
                source_tracker = 4,
                source_router = 8
            };

            boost::weak_ptr<transfer> find_transfer(const md4_hash& hash);
            virtual boost::weak_ptr<transfer> find_transfer(const std::string& filename);
            transfer_handle find_transfer_handle(const md4_hash& hash);
            peer_connection_handle find_peer_connection_handle(const net_identifier& np);
            peer_connection_handle find_peer_connection_handle(const md4_hash& np);
            std::vector<transfer_handle> get_transfers();
            std::vector<transfer_handle> get_active_transfers();

            /** add transfer to check queue */
            void queue_check_transfer(boost::shared_ptr<transfer> const& t);

            /** remove transfer from check queue */
            void dequeue_check_transfer(boost::shared_ptr<transfer> const& t);

            void close_connection(const peer_connection* p, const error_code& ec);

            session_status status() const;
            const tcp::endpoint& server() const;

            virtual void abort();

            bool is_aborted() const { return m_abort; }
            bool is_paused() const { return m_paused; }

            void pause();
            void resume();

            /** add/remove transfer from current thread directly */
            virtual transfer_handle add_transfer(add_transfer_params const&, error_code& ec);
            virtual void remove_transfer(const transfer_handle& h, int options);
            /** add/remove active transfer for this session */
            bool add_active_transfer(const boost::shared_ptr<transfer>& t);
            bool remove_active_transfer(const boost::shared_ptr<transfer>& t);
            void remove_active_transfer(transfer_map::iterator i);

            /** find peer connections */
            boost::intrusive_ptr<peer_connection> find_peer_connection(const net_identifier& np) const;
            boost::intrusive_ptr<peer_connection> find_peer_connection(const md4_hash& hash) const;
            peer_connection_handle add_peer_connection(net_identifier np, error_code& ec);

            int max_connections() const { return m_settings.connections_limit; }
            int num_connections() const { return m_connections.size(); }

            bool has_peer(const peer_connection* p) const
            {
                return std::find_if(
                    m_connections.begin(), m_connections.end(),
                    boost::bind(&boost::intrusive_ptr<peer_connection>::get, _1) == p)
                    != m_connections.end();
            }

            void add_redundant_bytes(size_type b, int reason)
            {
                LIBED2K_ASSERT(b > 0);
                m_total_redundant_bytes += b;
                m_redundant_bytes[reason] += b;
            }

            void add_failed_bytes(size_type b)
            {
                LIBED2K_ASSERT(b > 0);
                m_total_failed_bytes += b;
            }

            std::pair<char*, int> allocate_send_buffer(int size);
            void free_send_buffer(char* buf, int size);

            char* allocate_disk_buffer(char const* category);
            void free_disk_buffer(char* buf);

            char* allocate_z_buffer();
            void free_z_buffer(char* buf);

            bool can_write_to_disk() const { return m_disk_thread.can_write(); }

            std::string send_buffer_usage();

            void on_disk_queue();

            void on_tick(error_code const& e);

            // let transfers connect to peers if they want to
            // if there are any trasfers and any free slots
            void connect_new_peers();

            /** must be locked before access data in this class */
            typedef boost::mutex mutex_t;
            mutable mutex_t m_mutex;

            void setup_socket_buffers(tcp::socket& s);

            /** search file on server */
            void post_search_request(search_request& sr);

            /** after simple search call you can post request more search results */
            void post_search_more_result_request();

            /** this method simple send information packet to server and break search order */
            void post_cancel_search();

            /** request sources for file */
            void post_sources_request(const md4_hash& hFile, boost::uint64_t nSize);

            /**
              * when peer already exists - simple return it
              * when peer not exists connect and execute handshake
             */
            boost::intrusive_ptr<peer_connection> initialize_peer(client_id_type nIP, int nPort);

            void update_connections_limit();
            void update_rate_settings();
            void update_active_transfers();

			void start_natpmp();
			void start_upnp();

			void stop_natpmp();
			void stop_upnp();


			void set_external_address(address const& ip
			                                , int source_type, address const& source);
			address const& external_address() const { return m_external_address; }

			// TODO - should be implemented
            bool is_network_thread() const
            {
#if defined BOOST_HAS_PTHREADS
                    //if (m_network_thread == 0) return true;
                    //return m_network_thread == pthread_self();
#endif
                    return true;
            }


            struct external_ip_t
            {
                    external_ip_t(): sources(0), num_votes(0) {}

                    bool add_vote(md4_hash const& k, int type);
                    bool operator<(external_ip_t const& rhs) const
                    {
                            if (num_votes < rhs.num_votes) return true;
                            if (num_votes > rhs.num_votes) return false;
                            return sources < rhs.sources;
                    }

                    // this is a bloom filter of the IPs that have
                    // reported this address
                    bloom_filter<16> voters;
                    // this is the actual external address
                    address addr;
                    // a bitmask of sources the reporters have come from
                    boost::uint16_t sources;
                    // the total number of votes for this IP
                    boost::uint16_t num_votes;
            };

            // this is a bloom filter of all the IPs that have
            // been the first to report an external address. Each
            // IP only gets to add a new item once.
            bloom_filter<32> m_external_address_voters;
            std::vector<external_ip_t> m_external_addresses;
            address m_external_address;

#ifndef LIBED2K_DISABLE_DHT
            bool is_dht_running() const { return m_dht.get(); }
            entry dht_state() const;
            kad_state dht_estate() const;
            void add_dht_node_name(std::pair<std::string, int> const& node);
            void add_dht_node(std::pair<std::string, int> const& node, const std::string& id);
            void add_dht_router(std::pair<std::string, int> const& node);
            void set_dht_settings(dht_settings const& s);
            dht_settings const& get_dht_settings() const { return m_dht_settings; }
            void start_dht();
            void stop_dht();
            void start_dht(entry const& startup_state);

            entry m_dht_state;
            boost::intrusive_ptr<dht::dht_tracker> m_dht;
            dht_settings m_dht_settings;

            // these are used when starting the DHT
            // (and bootstrapping it), and then erased
            std::list<udp::endpoint> m_dht_router_nodes;

            // this announce timer is used
            // by the DHT.
            deadline_timer m_dht_announce_timer;
            void on_dht_router_name_lookup(error_code const& e
                    , tcp::resolver::iterator host);
            void find_keyword(const std::string& keyword);
            void find_sources(const md4_hash& hash, size_type size);

            void on_find_result(std::vector<tcp::endpoint> const& peers);
            void on_find_dht_source(const md4_hash& hash, uint8_t type, client_id_type ip, uint16_t port, client_id_type low_id);
            void on_find_dht_keyword(const md4_hash& h, const std::deque<kad_info_entry>&);
#endif

            // when as a socks proxy is used for peers, also
            // listen for incoming connections on a socks connection
            //boost::shared_ptr<socket_type> m_socks_listen_socket;
            //boost::uint16_t m_socks_listen_port;


            tcp::resolver m_host_resolver;
            boost::object_pool<peer> m_peer_pool;

            int add_port_mapping(int t, int external_port, int local_port);
            void delete_port_mapping(int handle);

            boost::object_pool<peer> m_ipv4_peer_pool;

            // this vector is used to store the block_info
            // objects pointed to by partial_piece_info returned
            // by torrent::get_download_queue.
            std::vector<block_info> m_block_info_storage;

            // this pool is used to allocate and recycle send
            // buffers from.
            boost::pool<> m_send_buffers;
            boost::mutex m_send_buffer_mutex;

            // this pool is used to allocate and recycle compressed data buffers
            boost::pool<> m_z_buffers;

            // used to skipping data in connections
            std::vector<char> m_skip_buffer;

            // the file pool that all storages in this session's
            // torrents uses. It sets a limit on the number of
            // open files by this session.
            // file pool must be destructed after the torrents
            // since they will still have references to it
            // when they are destructed.
            file_pool m_filepool;

            // handles disk io requests asynchronously
            // peers have pointers into the disk buffer
            // pool, and must be destructed before this
            // object. The disk thread relies on the file
            // pool object, and must be destructed before
            // m_files. The disk io thread posts completion
            // events to the io service, and needs to be
            // constructed after it.
            disk_io_thread m_disk_thread;

            // this is a list of half-open tcp connections
            // (only outgoing connections)
            // this has to be one of the last
            // members to be destructed
            libed2k::connection_queue m_half_open;

            // the bandwidth manager is responsible for
            // handing out bandwidth to connections that
            // asks for it, it can also throttle the
            // rate.
            bandwidth_manager m_download_rate;
            bandwidth_manager m_upload_rate;

            // the global rate limiter bandwidth channels
            bandwidth_channel m_download_channel;
            bandwidth_channel m_upload_channel;

            bandwidth_channel* m_bandwidth_channel[2];

            // ed2k server connection
            boost::intrusive_ptr<server_connection> m_server_connection;

            // the index of the transfers that will be offered to
            // connect to a peer next time on_tick is called.
            // This implements a round robin.
            cyclic_iterator<transfer_map> m_next_connect_transfer;

            // this maps sockets to their peer_connection
            // object. It is the complete list of all connected
            // peers.
            connection_map m_connections;

            // filters incoming connections
            ip_filter m_ip_filter;

            // the ip-address of the interface
            // we are supposed to listen on.
            // if the ip is set to zero, it means
            // that we should let the os decide which
            // interface to listen on
            tcp::endpoint m_listen_interface;

            typedef aux::listen_socket_t listen_socket_t;

            // possibly we will be listening on multiple interfaces
            // so we might need more than one listen socket
            std::list<listen_socket_t> m_listen_sockets;

            void open_new_incoming_socks_connection();

            listen_socket_t setup_listener(tcp::endpoint ep, bool v6_only = false);

            // is true if the session is paused
            bool m_paused;

            ptime m_created;
            int session_time() const { return total_seconds(time_now() - m_created); }

            duration_timer m_second_timer;
            // the timer used to fire the tick
            deadline_timer m_timer;
            ptime m_last_tick;
            // total redundant and failed bytes
            size_type m_total_failed_bytes;
            size_type m_total_redundant_bytes;

            // redundant bytes per category
            size_type m_redundant_bytes[7];

            int m_queue_pos;

            rate_limited_udp_socket m_udp_socket;

            boost::intrusive_ptr<natpmp> m_natpmp;
            boost::intrusive_ptr<upnp> m_upnp;

            // mask is a bitmask of which protocols to remap on:
            // 1: NAT-PMP
            // 2: UPnP
            void remap_tcp_ports(boost::uint32_t mask, int tcp_port, int ssl_port);

            // 0 is natpmp 1 is upnp
            int m_tcp_mapping[2];
            int m_udp_mapping[2];
#ifdef LIBED2K_USE_OPENSSL
            int m_ssl_mapping[2];
#endif

            // the main working thread
            // !!! should be last in the member list
            boost::scoped_ptr<boost::thread> m_thread;
        };
    }
}

#endif
