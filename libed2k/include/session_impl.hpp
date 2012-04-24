// ed2k session network thread

#ifndef __LIBED2K_SESSION_IMPL__
#define __LIBED2K_SESSION_IMPL__

#include <string>

#include <map>
#include <set>

#include "libtorrent/torrent_handle.hpp"
#include "libtorrent/socket.hpp"
#include "libtorrent/peer_connection.hpp"
#include "libtorrent/config.hpp"
#include "libtorrent/assert.hpp"
#include "libtorrent/identify_client.hpp"
#include "libtorrent/stat.hpp"

#include "fingerprint.hpp"
#include "md4_hash.hpp"
#include "transfer_handle.hpp"
#include "session_settings.hpp"
#include "types.hpp"
#include "util.hpp"
#include "alert.hpp"

namespace libed2k {

    class peer_connection;
    class server_connection;
    class transfer;
    class base_socket;
    class add_transfer_params;

    namespace aux {

        class session_impl: boost::noncopyable
        {
        public:
            // the size of each allocation that is chained in the send buffer
            enum { send_buffer_size = 128 };

            typedef std::map<md4_hash, boost::shared_ptr<transfer> > transfer_map;
            typedef std::set<boost::intrusive_ptr<peer_connection> > connection_map;

            session_impl(const fingerprint& id, const char* listen_interface,
                         const session_settings& settings);
            ~session_impl();

            // main thread entry point
            void operator()();

            void open_listen_port();

            void async_accept(boost::shared_ptr<tcp::acceptor> const& listener);
            void on_accept_connection(boost::shared_ptr<base_socket> const& s,
                                      boost::weak_ptr<tcp::acceptor> listener, 
                                      error_code const& e);

            void incoming_connection(boost::shared_ptr<base_socket> const& s);

            boost::weak_ptr<transfer> find_transfer(const md4_hash& hash);

            unsigned short listen_port() const;

            void abort();

            bool is_aborted() const { return m_abort; }
            bool is_paused() const { return m_paused; }

            transfer_handle add_transfer(add_transfer_params const&, error_code& ec);
            std::vector<transfer_handle> add_transfer_dir(
                const fs::path& dir, error_code& ec);

            int max_connections() const { return m_max_connections; }
            int num_connections() const { return m_connections.size(); }

            std::pair<char*, int> allocate_buffer(int size);
            void free_buffer(char* buf, int size);

            char* allocate_disk_buffer(char const* category);
            void free_disk_buffer(char* buf);

            const session_settings& settings() const { return m_settings; }
            session_settings& settings() { return m_settings; }

            void on_disk_queue();

            void on_tick(error_code const& e);

            bool has_active_transfer() const;

            // let transfers connect to peers if they want to
            // if there are any trasfers and any free slots
            void connect_new_peers();

            // must be locked to access the data
            // in this struct
            mutable boost::mutex m_mutex;

            void setup_socket_buffers(tcp::socket& s);

            /**
              * alerts
             */
            std::auto_ptr<alert> pop_alert();
            void set_alert_mask(boost::uint32_t m);
            size_t set_alert_queue_size_limit(size_t queue_size_limit_);
            void set_alert_dispatch(boost::function<void(alert const&)> const&);
            alert const* wait_for_alert(time_duration max_wait);


            boost::object_pool<peer> m_peer_pool;

            // this vector is used to store the block_info
            // objects pointed to by partial_piece_info returned
            // by torrent::get_download_queue.
            std::vector<libtorrent::block_info> m_block_info_storage;

            // this pool is used to allocate and recycle send
            // buffers from.
            boost::pool<> m_send_buffers;

            boost::mutex m_send_buffer_mutex;

            // the file pool that all storages in this session's
            // torrents uses. It sets a limit on the number of
            // open files by this session.
            // file pool must be destructed after the torrents
            // since they will still have references to it
            // when they are destructed.
            libtorrent::file_pool m_filepool;

            // this is where all active sockets are stored.
            // the selector can sleep while there's no activity on
            // them
            mutable boost::asio::io_service m_io_service;

            // handles delayed alerts
            alert_manager m_alerts;

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

            // ed2k server connection
            boost::intrusive_ptr<server_connection> m_server_connection;

            transfer_map m_transfers;

            // the index of the torrent that will be offered to
            // connect to a peer next time on_tick is called.
            // This implements a round robin.
            cyclic_iterator<transfer_map> m_next_connect_transfer;

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

            // the settings for the client
            session_settings m_settings;

            // set to true when the session object
            // is being destructed and the thread
            // should exit
            bool m_abort;

			// is true if the session is paused
			bool m_paused;

			// the max number of connections, as set by the user
			int m_max_connections;

            ptime m_last_second_tick;

            // the timer used to fire the tick
            boost::asio::deadline_timer m_timer;

            // the main working thread
            // !!! should be last in the member list
            boost::scoped_ptr<boost::thread> m_thread;

        };
    }
}

#endif
