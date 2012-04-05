// ed2k session network thread

#ifndef __LIBED2K_SESSION_IMPL__
#define __LIBED2K_SESSION_IMPL__

#include <string>

#include <boost/filesystem/path.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/pool/object_pool.hpp>

#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/policy.hpp>
#include <libtorrent/file_pool.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/disk_io_thread.hpp>
#include <libtorrent/connection_queue.hpp>
#include <libtorrent/bandwidth_manager.hpp>
#include <libtorrent/ip_filter.hpp>
#include <libtorrent/debug.hpp>

#include "fingerprint.hpp"
#include "peer_connection.hpp"
#include "server_manager.hpp"
#include "md4_hash.hpp"
#include "transfer.hpp"


namespace libed2k {

    typedef boost::asio::ip::tcp tcp;
    typedef libtorrent::logger logger;
    namespace fs = boost::filesystem;

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

            unsigned short listen_port() const;

        private:

            void on_disk_queue();

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

            // the bandwidth manager is responsible for
            // handing out bandwidth to connections that
            // asks for it, it can also throttle the
            // rate.
            libtorrent::bandwidth_manager<peer_connection> m_download_rate;
            libtorrent::bandwidth_manager<peer_connection> m_upload_rate;

            // the global rate limiter bandwidth channels
            libtorrent::bandwidth_channel m_download_channel;
            libtorrent::bandwidth_channel m_upload_channel;

            libtorrent::bandwidth_channel* m_bandwidth_channel[2];

            server_manager m_server_manager;
            transfer_map m_transfers;
            typedef std::list<boost::shared_ptr<transfer> > check_queue_t;

            // this maps sockets to their peer_connection
            // object. It is the complete list of all connected
            // peers.
            connection_map m_connections;

            // filters incoming connections
            libtorrent::ip_filter m_ip_filter;

            // filters outgoing connections
            libtorrent::port_filter m_port_filter;

            // the ip-address of the interface
            // we are supposed to listen on.
            // if the ip is set to zero, it means
            // that we should let the os decide which
            // interface to listen on
            tcp::endpoint m_listen_interface;

            // if we're listening on an IPv6 interface
            // this is one of the non local IPv6 interfaces
            // on this machine
            tcp::endpoint m_ipv4_interface;

            // set to true when the session object
            // is being destructed and the thread
            // should exit
            volatile bool m_abort;

			// is true if the session is paused
			bool m_paused;

			// the max number of unchoked peers as set by the user
			int m_max_uploads;

			// the number of unchoked peers as set by the auto-unchoker
			// this should always be >= m_max_uploads
			int m_allowed_upload_slots;

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
