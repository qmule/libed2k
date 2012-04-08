
#include <sys/resource.h>

#include <libtorrent/peer_connection.hpp>

#include "session.hpp"
#include "session_impl.hpp"
#include "transfer_handle.hpp"
#include "transfer.hpp"
#include "error_code.hpp"
#include "peer_connection.hpp"

using namespace libed2k;
using namespace libed2k::aux;

session_impl::session_impl(int lst_port, const char* listen_interface,
                           const fingerprint& id, const std::string& logpath):
    m_ipv4_peer_pool(500),
    m_send_buffers(send_buffer_size),
    m_filepool(40),
    m_io_service(),
	m_host_resolver(m_io_service),
    m_alerts(m_io_service),
    m_disk_thread(m_io_service, boost::bind(&session_impl::on_disk_queue, this),
                  m_filepool),
    m_half_open(m_io_service),
    m_server_manager(*this),
    m_abort(false),
    m_paused(false),
    m_max_connections(200),
    m_logpath(logpath)
{
    error_code ec;
    m_listen_interface = tcp::endpoint(
        libtorrent::address::from_string(listen_interface, ec), lst_port);
    TORRENT_ASSERT(!ec);

#ifdef WIN32
    // windows XP has a limit on the number of
    // simultaneous half-open TCP connections
    // here's a table:

    // windows version       half-open connections limit
    // --------------------- ---------------------------
    // XP sp1 and earlier    infinite
    // earlier than vista    8
    // vista sp1 and earlier 5
    // vista sp2 and later   infinite

    // windows release                     version number
    // ----------------------------------- --------------
    // Windows 7                           6.1
    // Windows Server 2008 R2              6.1
    // Windows Server 2008                 6.0
    // Windows Vista                       6.0
    // Windows Server 2003 R2              5.2
    // Windows Home Server                 5.2
    // Windows Server 2003                 5.2
    // Windows XP Professional x64 Edition 5.2
    // Windows XP                          5.1
    // Windows 2000                        5.0

    OSVERSIONINFOEX osv;
    memset(&osv, 0, sizeof(osv));
    osv.dwOSVersionInfoSize = sizeof(osv);
    GetVersionEx((OSVERSIONINFO*)&osv);

    // the low two bytes of windows_version is the actual
    // version.
    boost::uint32_t windows_version
        = ((osv.dwMajorVersion & 0xff) << 16)
        | ((osv.dwMinorVersion & 0xff) << 8)
        | (osv.wServicePackMajor & 0xff);

    // this is the format of windows_version
    // xx xx xx
    // |  |  |
    // |  |  + service pack version
    // |  + minor version
    // + major version

    // the least significant byte is the major version
    // and the most significant one is the minor version
    if (windows_version >= 0x060100)
    {
        // windows 7 and up doesn't have a half-open limit
        m_half_open.limit(0);
    }
    else if (windows_version >= 0x060002)
    {
        // on vista SP 2 and up, there's no limit
        m_half_open.limit(0);
    }
    else if (windows_version >= 0x060000)
    {
        // on vista the limit is 5 (in home edition)
        m_half_open.limit(4);
    }
    else if (windows_version >= 0x050102)
    {
        // on XP SP2 the limit is 10
        m_half_open.limit(9);
    }
    else
    {
        // before XP SP2, there was no limit
        m_half_open.limit(0);
    }
#endif

    m_logger = create_log("main_session", listen_port(), false);
    (*m_logger) << libtorrent::time_now_string() << "\n";

#if defined TORRENT_BSD || defined TORRENT_LINUX
    // ---- auto-cap open files ----

    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
    {
        (*m_logger) << libtorrent::time_now_string() << " max number of open files: "
                    << rl.rlim_cur << "\n";

        // deduct some margin for epoll/kqueue, log files,
        // futexes, shared objects etc.
        rl.rlim_cur -= 20;

        // 80% of the available file descriptors should go
        m_max_connections = (std::min)(m_max_connections, int(rl.rlim_cur * 8 / 10));
        // 20% goes towards regular files
        m_filepool.resize((std::min)(m_filepool.size_limit(), int(rl.rlim_cur * 2 / 10)));

        (*m_logger) << libtorrent::time_now_string() << "   max connections: "
                    << m_max_connections << "\n";
        (*m_logger) << libtorrent::time_now_string() << "   max files: "
                    << m_filepool.size_limit() << "\n";
    }
#endif // TORRENT_BSD || TORRENT_LINUX

    m_io_service.post(boost::bind(&session_impl::on_tick, this, ec));

    m_thread.reset(new boost::thread(boost::ref(*this)));
}

void session_impl::operator()()
{
    // main session thread

    eh_initializer();

    if (m_listen_interface.port() != 0)
    {
        boost::mutex::scoped_lock l(m_mutex);
        open_listen_port();
    }

    bool stop_loop = false;
    while (!stop_loop)
    {
        error_code ec;
        m_io_service.run(ec);
        TORRENT_ASSERT(m_abort == true);
        if (ec)
        {
            std::cerr << ec.message() << "\n";
            std::string err = ec.message();

            TORRENT_ASSERT(false);
        }
        m_io_service.reset();

        boost::mutex::scoped_lock l(m_mutex);
        stop_loop = m_abort;
    }

    boost::mutex::scoped_lock l(m_mutex);
    m_transfers.clear();
}

void session_impl::open_listen_port()
{
    // close the open listen sockets
    m_listen_sockets.clear();

    // we should only open a single listen socket, that
    // binds to the given interface
    listen_socket_t s = setup_listener(m_listen_interface);

    if (s.sock)
    {
        m_listen_sockets.push_back(s);
        async_accept(s.sock);
    }

    m_logger = create_log("main_session", listen_port(), false);
}

void session_impl::async_accept(boost::shared_ptr<tcp::acceptor> const& listener)
{
    boost::shared_ptr<tcp::socket> c(new tcp::socket(m_io_service));
    listener->async_accept(*c, bind(&session_impl::on_accept_connection, this, c,
                                    boost::weak_ptr<tcp::acceptor>(listener), _1));
}

void session_impl::on_accept_connection(boost::shared_ptr<tcp::socket> const& s,
                                        boost::weak_ptr<tcp::acceptor> listen_socket,
                                        error_code const& e)
{
    boost::shared_ptr<tcp::acceptor> listener = listen_socket.lock();
    if (!listener) return;

    if (e == boost::asio::error::operation_aborted) return;

    if (m_abort) return;

    error_code ec;
    if (e)
    {
        tcp::endpoint ep = listener->local_endpoint(ec);

        std::string msg =
            "error accepting connection on '" +
            libtorrent::print_endpoint(ep) + "' " + e.message();
        (*m_logger) << msg << "\n";

#ifdef TORRENT_WINDOWS
        // Windows sometimes generates this error. It seems to be
        // non-fatal and we have to do another async_accept.
        if (e.value() == ERROR_SEM_TIMEOUT)
        {
            async_accept(listener);
            return;
        }
#endif
#ifdef TORRENT_BSD
        // Leopard sometimes generates an "invalid argument" error. It seems to be
        // non-fatal and we have to do another async_accept.
        if (e.value() == EINVAL)
        {
            async_accept(listener);
            return;
        }
#endif
        if (m_alerts.should_post<listen_failed_alert>())
            m_alerts.post_alert(listen_failed_alert(ep, e));
        return;
    }

    async_accept(listener);
    incoming_connection(s);
}

void session_impl::incoming_connection(boost::shared_ptr<tcp::socket> const& s)
{
    error_code ec;
    // we got a connection request!
    tcp::endpoint endp = s->remote_endpoint(ec);

    if (ec)
    {
        (*m_logger) << endp << " <== INCOMING CONNECTION FAILED, could "
            "not retrieve remote endpoint " << ec.message() << "\n";
        return;
    }

    (*m_logger) << libtorrent::time_now_string() << " <== INCOMING CONNECTION "
                << endp << "\n";

    // don't allow more connections than the max setting
    if (num_connections() >= max_connections())
    {
        //TODO: fire alert here

        (*m_logger) << "number of connections limit exceeded (conns: "
                    << num_connections() << ", limit: " << max_connections()
                    << "), connection rejected\n";

        return;
    }

    // check if we have any active transfers
    // if we don't reject the connection
    if (m_transfers.empty())
    {
        (*m_logger) << " There are no ttansfers, disconnect\n";
        return;
    }

    if (!has_active_transfer())
    {
        (*m_logger) << " There are no _active_ torrents, disconnect\n";
        return;
    }

    setup_socket_buffers(*s);

    boost::intrusive_ptr<peer_connection> c(new peer_connection(*this, s, endp));

    if (!c->is_disconnecting())
    {
        m_connections.insert(c);
        c->start();
    }
}

boost::weak_ptr<transfer> session_impl::find_transfer(const md4_hash& hash)
{
    std::map<md4_hash, boost::shared_ptr<transfer> >::iterator i = m_transfers.find(hash);

    if (i != m_transfers.end()) return i->second;

    return boost::weak_ptr<transfer>();
}

transfer_handle session_impl::add_transfer(add_transfer_params const& params, error_code& ec)
{
    TORRENT_ASSERT(!params.save_path.empty());

    if (is_aborted())
    {
        ec = errors::session_is_closing;
        return transfer_handle();
    }

    // is the transfer already active?
    boost::shared_ptr<transfer> transfer_ptr = find_transfer(params.info_hash).lock();
    if (transfer_ptr)
    {
        if (!params.duplicate_is_error)
            return transfer_handle(transfer_ptr);

        ec = errors::duplicate_torrent;
        return transfer_handle();
    }

    int queue_pos = 0;
    for (transfer_map::const_iterator i = m_transfers.begin(),
             end(m_transfers.end()); i != end; ++i)
    {
        int pos = i->second->queue_position();
        if (pos >= queue_pos) queue_pos = pos + 1;
    }

    transfer_ptr.reset(new transfer(*this, m_listen_interface, queue_pos, params));
    transfer_ptr->start();

    m_transfers.insert(std::make_pair(params.info_hash, transfer_ptr));

    return transfer_handle(transfer_ptr);
}

unsigned short session_impl::listen_port() const
{
}

void session_impl::on_disk_queue()
{
}

void session_impl::on_tick(error_code const& e)
{
    boost::mutex::scoped_lock l(m_mutex);

    if (m_abort) return;

    if (e == boost::asio::error::operation_aborted) return;

    if (e)
    {
        (*m_logger) << "*** TICK TIMER FAILED " << e.message() << "\n";
        ::abort();
        return;
    }

    // --------------------------------------------------------------
    // check for incoming connections that might have timed out
    // --------------------------------------------------------------
    // TODO: should it be implemented?


    // --------------------------------------------------------------
    // connect new peers
    // --------------------------------------------------------------
    // TODO: implement

    // let transfers connect to peers if they want to
    // if there are any transfers and any free slots

    // --------------------------------------------------------------
    // disconnect peers when we have too many
    // --------------------------------------------------------------
    // TODO: should it be implemented?

}

bool session_impl::has_active_transfer() const
{
    for (transfer_map::const_iterator i = m_transfers.begin(), end(m_transfers.end());
         i != end; ++i)
    {
        if (!i->second->is_paused())
        {
            return true;
        }
    }

    return false;
}

void session_impl::setup_socket_buffers(tcp::socket& s)
{
}

session_impl::listen_socket_t session_impl::setup_listener(
    tcp::endpoint ep, bool v6_only)
{
}

boost::shared_ptr<logger> session_impl::create_log(
    std::string const& name, int instance, bool append)
{
    return boost::shared_ptr<logger>(
        new logger(m_logpath, name + ".log", instance, append));
}
