// ed2k session network thread

#ifndef __LIBED2K_SESSION_IMPL__
#define __LIBED2K_SESSION_IMPL__

#include <string>

#include <libtorrent/alert_types.hpp>

#include "fingerprint.hpp"
#include "server_manager.hpp"
#include "md4_hash.hpp"
#include "transfer_handle.hpp"

namespace libed2k {

    typedef boost::asio::ip::tcp tcp;
    typedef libtorrent::logger logger;
    typedef libtorrent::aux::eh_initializer eh_initializer;
    typedef libed2k::error_code error_code;
    typedef libtorrent::stream_socket stream_socket;
    typedef libtorrent::ip_filter ip_filter;
    typedef libtorrent::peer_blocked_alert peer_blocked_alert;
    typedef libtorrent::listen_failed_alert listen_failed_alert;

    namespace errors = libtorrent::errors;
    namespace fs = boost::filesystem;

    class peer_connection;
    class transfer;
    struct add_transfer_params;

    namespace aux {

        struct session_impl: boost::noncopyable
        {

            // the size of each allocation that is chained in the send buffer
            enum { send_buffer_size = 128 };

            typedef std::map<md4_hash, boost::shared_ptr<transfer> > transfer_map;
            typedef std::set<boost::intrusive_ptr<peer_connection> > connection_map;

            session_impl(int listen_port, const char* listen_interface,
                         const fingerprint& id, const std::string& logpath);

            // main thread entry point
            void operator()();

            void open_listen_port();

            void async_accept(boost::shared_ptr<tcp::acceptor> const& listener);
            void on_accept_connection(boost::shared_ptr<tcp::socket> const& s,
                                      boost::weak_ptr<tcp::acceptor> listener, 
                                      error_code const& e);

            void incoming_connection(boost::shared_ptr<tcp::socket> const& s);

            boost::weak_ptr<transfer> find_transfer(const md4_hash& hash);

            unsigned short listen_port() const;

            bool is_aborted() const { return m_abort; }
            bool is_paused() const { return m_paused; }

            transfer_handle add_transfer(add_transfer_params const&, error_code& ec);

            int max_connections() const { return m_max_connections; }
            int num_connections() const { return m_connections.size(); }

        private:

            void on_disk_queue();

            void on_tick(error_code const& e);

            bool has_active_transfer() const;

            // must be locked to access the data
            // in this struct
            mutable boost::mutex m_mutex;

            void setup_socket_buffers(tcp::socket& s);

            boost::object_pool<libtorrent::policy::ipv4_peer> m_ipv4_peer_pool;

            // this vector is used to store the block_info
            // objects pointed to by partial_piece_info returned
            // by torrent::get_download_queue.
            std::vector<libtorrent::block_info> m_block_info_storage;

            // this pool is used to allocate and recycle send
            // buffers from.
            boost::pool<> m_send_buffers;

            // the file pool that all storages in this session's
            // torrents uses. It sets a limit on the number of
            // open files by this session.
            // file pool must be destructed after the torrents
            // since they will still have references to it
            // when they are destructed.
            libtorrent::file_pool m_files;

            // this is where all active sockets are stored.
            // the selector can sleep while there's no activity on
            // them
            mutable boost::asio::io_service m_io_service;

            tcp::resolver m_host_resolver;

            // handles delayed alerts
            libtorrent::alert_manager m_alerts;

            // handles disk io requests asynchronously
            // peers have pointers into the disk buffer
            // pool, and must be destructed before this
            // object. The disk thread relies on the file
            // pool object, and must be destructed before
            // m_files. The disk io thread posts completion
            // events to the io service, and needs to be
            // constructed after it.
            libtorrent::disk_io_thread m_disk_thread;

            // this is a list of half-open tcp connections
            // (only outgoing connections)
            // this has to be one of the last
            // members to be destructed
            libtorrent::connection_queue m_half_open;

            server_manager m_server_manager;
            transfer_map m_transfers;
            typedef std::list<boost::shared_ptr<transfer> > check_queue_t;

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

            typedef libtorrent::aux::session_impl::listen_socket_t listen_socket_t;

            // possibly we will be listening on multiple interfaces
            // so we might need more than one listen socket
            std::list<listen_socket_t> m_listen_sockets;

            void open_new_incoming_socks_connection();

            listen_socket_t setup_listener(tcp::endpoint ep, bool v6_only = false);

            // set to true when the session object
            // is being destructed and the thread
            // should exit
            bool m_abort;

			// is true if the session is paused
			bool m_paused;

			// the max number of connections, as set by the user
			int m_max_connections;

			boost::shared_ptr<logger> create_log(std::string const& name, 
                                                 int instance, bool append = true);

            fs::path m_logpath;

        public:
            boost::shared_ptr<logger> m_logger;
        private:

            // the main working thread
            // !!! should be last in the member list
            boost::scoped_ptr<boost::thread> m_thread;

        };
    }
}

#endif
