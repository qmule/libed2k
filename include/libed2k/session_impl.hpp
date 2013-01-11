
#ifndef __LIBED2K_SESSION_IMPL__
#define __LIBED2K_SESSION_IMPL__

#include <string>

#include <map>
#include <set>

#include <boost/pool/object_pool.hpp>

#include "libed2k/socket.hpp"
#include "libed2k/stat.hpp"
#include "libed2k/fingerprint.hpp"
#include "libed2k/md4_hash.hpp"
#include "libed2k/transfer_handle.hpp"
#include "libed2k/peer_connection_handle.hpp"
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

namespace libed2k {

    class peer_connection;
    class server_connection;
    class transfer;
    class add_transfer_params;
    struct transfer_handle;
    struct listen_socket_t;

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

        /**
         * class used for testing
         */
        class session_impl_base : boost::noncopyable, initialize_timer
        {
        public:
            typedef std::map<std::pair<std::string, boost::uint32_t>, md4_hash> transfer_filename_map;
            typedef std::map<md4_hash, boost::shared_ptr<transfer> > transfer_map;

            session_impl_base(const session_settings& settings);
            virtual ~session_impl_base();

            virtual void abort();

            /**
             *  add transfer from another thread
             */
            virtual void post_transfer(add_transfer_params const& );
            virtual transfer_handle add_transfer(add_transfer_params const&, error_code& ec) = 0;
            virtual boost::weak_ptr<transfer> find_transfer(const std::string& filename) = 0;
            virtual void remove_transfer(const transfer_handle& h, int options) = 0;
            virtual std::vector<transfer_handle> get_transfers() = 0;

            /**
             * alerts
             */
            std::auto_ptr<alert> pop_alert();
            void set_alert_mask(boost::uint32_t m);
            size_t set_alert_queue_size_limit(size_t queue_size_limit_);
            void set_alert_dispatch(boost::function<void(alert const&)> const&);
            alert const* wait_for_alert(time_duration max_wait);

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
            transfer_map m_transfers;

            typedef std::list<boost::shared_ptr<transfer> > check_queue_t;

            // this has all torrents that wants to be checked in it
            check_queue_t m_queued_for_checking;

            // statistics gathered from all transfers.
            stat m_stat;

            // handles delayed alerts
            alert_manager m_alerts;

            /**
             * file hasher closed in self thread
             */
            transfer_params_maker    m_tpm;
        };

        class session_impl : public session_impl_base
        {
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

            bool listen_on(int port, const char* net_interface);
            bool is_listening() const;
            unsigned short listen_port() const;
            char server_connection_state() const;

            void update_disk_thread_settings();

            void async_accept(boost::shared_ptr<tcp::acceptor> const& listener);
            void on_accept_connection(boost::shared_ptr<tcp::socket> const& s,
                                      boost::weak_ptr<tcp::acceptor> listener,
                                      error_code const& e);

            void incoming_connection(boost::shared_ptr<tcp::socket> const& s);

            boost::weak_ptr<transfer> find_transfer(const md4_hash& hash);
            virtual boost::weak_ptr<transfer> find_transfer(const std::string& filename);
            transfer_handle find_transfer_handle(const md4_hash& hash);
            peer_connection_handle find_peer_connection_handle(const net_identifier& np);
            peer_connection_handle find_peer_connection_handle(const md4_hash& np);
            virtual std::vector<transfer_handle> get_transfers();

            /**
              * add transfer to check queue
             */
            void queue_check_transfer(boost::shared_ptr<transfer> const& t);

            /**
              * remove transfer from check queue
             */
            void dequeue_check_transfer(boost::shared_ptr<transfer> const& t);

            void close_connection(const peer_connection* p, const error_code& ec);

            session_status status() const;
            const tcp::endpoint& server() const;

            virtual void abort();

            bool is_aborted() const { return m_abort; }
            bool is_paused() const { return m_paused; }

            void pause();
            void resume();

            /** add transfer from current thread directly */
            virtual transfer_handle add_transfer(add_transfer_params const&, error_code& ec);
            virtual void remove_transfer(const transfer_handle& h, int options);

            /**
             * find peer connections
             */
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

            std::pair<char*, int> allocate_buffer(int size);
            void free_buffer(char* buf, int size);

            char* allocate_disk_buffer(char const* category);
            void free_disk_buffer(char* buf);
            bool can_write_to_disk() const { return m_disk_thread.can_write(); }

            std::string buffer_usage();

            void on_disk_queue();

            void on_tick(error_code const& e);

            bool has_active_transfer() const;

            // let transfers connect to peers if they want to
            // if there are any trasfers and any free slots
            void connect_new_peers();

            /**
              * must be locked before access data in this class
             */
            typedef boost::mutex mutex_t;
            mutable mutex_t m_mutex;

            void setup_socket_buffers(tcp::socket& s);

            /**
              * search file on server
             */
            void post_search_request(search_request& sr);

            /**
              * after simple search call you can post request more search results
             */
            void post_search_more_result_request();

            /**
              * this method simple send information packet to server and break search order
             */
            void post_cancel_search();

            /**
              * request sources for file
             */
            void post_sources_request(const md4_hash& hFile, boost::uint64_t nSize);

            /**
              * when peer already exists - simple return it
              * when peer not exists connect and execute handshake
             */
            boost::intrusive_ptr<peer_connection> initialize_peer(client_id_type nIP, int nPort);

            /**
              * announce transfers
              * will perform only when server connection online
             */
            void announce(int tick_interval_ms);

            /**
              * perform reconnect to server
              * will perform only when server connection offline
             */
            void reconnect(int tick_interval_ms);
            void server_conn_start();
            void server_conn_stop();

            void update_connections_limit();
            void update_rate_settings();

            boost::object_pool<peer> m_peer_pool;

            // this vector is used to store the block_info
            // objects pointed to by partial_piece_info returned
            // by torrent::get_download_queue.
            std::vector<block_info> m_block_info_storage;

            // this pool is used to allocate and recycle send
            // buffers from.
            boost::pool<> m_send_buffers;
            boost::mutex m_send_buffer_mutex;

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

            // the index of the torrent that will be offered to
            // connect to a peer next time on_tick is called.
            // This implements a round robin.
            cyclic_iterator<transfer_map> m_next_connect_transfer;

            // this maps sockets to their peer_connection
            // object. It is the complete list of all connected
            // peers.
            connection_map m_connections;

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

            duration_timer m_second_timer;
            // the timer used to fire the tick
            deadline_timer m_timer;

            ptime m_last_tick;
            // duration in milliseconds since last server connection was executed
            int m_last_connect_duration;
            // duration in milliseconds since last announce check was performed
            int m_last_announce_duration;
            // ismod extension - share user as file
            bool m_user_announced;
            // last measured server connection state
            char m_server_connection_state;

            // total redundant and failed bytes
            size_type m_total_failed_bytes;
            size_type m_total_redundant_bytes;

            // redundant bytes per category
            size_type m_redundant_bytes[7];

            // the main working thread
            // !!! should be last in the member list
            boost::scoped_ptr<boost::thread> m_thread;
        };
    }
}

#endif
