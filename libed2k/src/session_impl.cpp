
#ifndef WIN32
#include <sys/resource.h>
#endif

#include <libtorrent/peer_connection.hpp>
#include <libtorrent/socket.hpp>

#include "session.hpp"
#include "session_impl.hpp"
#include "transfer_handle.hpp"
#include "transfer.hpp"
#include "peer_connection.hpp"
#include "server_connection.hpp"
#include "log.hpp"

using namespace libed2k;
using namespace libed2k::aux;

session_impl::session_impl(const fingerprint& id, int lst_port,
                           const char* listen_interface,
                           const session_settings& settings):
    m_ipv4_peer_pool(500),
    m_send_buffers(send_buffer_size),
    m_filepool(40),
    m_io_service(),
	m_host_resolver(m_io_service),
    m_alerts(m_io_service),
    m_disk_thread(m_io_service, boost::bind(&session_impl::on_disk_queue, this),
                  m_filepool), // TODO - check it!
    m_half_open(m_io_service),
    m_server_connection(new server_connection(*this)), // TODO - check it
    m_settings(settings),
    m_abort(false),
    m_paused(false),
    m_max_connections(200)
{
    LDBG_ << "*** create ed2k session ***";

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

#if defined TORRENT_BSD || defined TORRENT_LINUX
    // ---- auto-cap open files ----

    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
    {
        LDBG_ << "max number of open files: " << rl.rlim_cur;

        // deduct some margin for epoll/kqueue, log files,
        // futexes, shared objects etc.
        rl.rlim_cur -= 20;

        // 80% of the available file descriptors should go
        m_max_connections = (std::min)(m_max_connections, int(rl.rlim_cur * 8 / 10));
        // 20% goes towards regular files
        m_filepool.resize((std::min)(m_filepool.size_limit(), int(rl.rlim_cur * 2 / 10)));

        LDBG_ << "max connections: " << m_max_connections;
        LDBG_ << "max files: " << m_filepool.size_limit();
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

    m_server_connection->start();

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

}

void session_impl::async_accept(boost::shared_ptr<tcp::acceptor> const& listener)
{
    boost::shared_ptr<base_socket> c(new base_socket(m_io_service));
    listener->async_accept(c->socket(),
                           bind(&session_impl::on_accept_connection, this, c,
                                boost::weak_ptr<tcp::acceptor>(listener), _1));
}

void session_impl::on_accept_connection(boost::shared_ptr<base_socket> const& s,
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
        LDBG_ << msg << "\n";

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

void session_impl::incoming_connection(boost::shared_ptr<base_socket> const& s)
{
    error_code ec;
    // we got a connection request!
    tcp::endpoint endp = s->socket().remote_endpoint(ec);

    if (ec)
    {
        LERR_ << endp << " <== INCOMING CONNECTION FAILED, could "
            "not retrieve remote endpoint " << ec.message();
        return;
    }

    LDBG_ << "<== INCOMING CONNECTION " << endp;

    // don't allow more connections than the max setting
    if (num_connections() >= max_connections())
    {
        //TODO: fire alert here

        LDBG_ << "number of connections limit exceeded (conns: "
              << num_connections() << ", limit: " << max_connections()
              << "), connection rejected";

        return;
    }

    // check if we have any active transfers
    // if we don't reject the connection
    if (m_transfers.empty())
    {
        LDBG_ << " There are no ttansfers, disconnect\n";
        return;
    }

    if (!has_active_transfer())
    {
        LDBG_ << " There are no _active_ torrents, disconnect\n";
        return;
    }

    setup_socket_buffers(s->socket());

    boost::intrusive_ptr<peer_connection> c(new peer_connection(*this, s, endp, 0));

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

        ec = errors::duplicate_transfer;
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

std::pair<char*, int> session_impl::allocate_buffer(int size)
{
    int num_buffers = (size + send_buffer_size - 1) / send_buffer_size;

    boost::mutex::scoped_lock l(m_send_buffer_mutex);

    return std::make_pair((char*)m_send_buffers.ordered_malloc(num_buffers),
                          num_buffers * send_buffer_size);
}

void session_impl::free_buffer(char* buf, int size)
{
    int num_buffers = size / send_buffer_size;

    boost::mutex::scoped_lock l(m_send_buffer_mutex);
    m_send_buffers.ordered_free(buf, num_buffers);
}

char* session_impl::allocate_disk_buffer(char const* category)
{
    return m_disk_thread.allocate_buffer(category);
}

void session_impl::free_disk_buffer(char* buf)
{
    m_disk_thread.free_buffer(buf);
}

unsigned short session_impl::listen_port() const
{
    if (m_listen_sockets.empty()) return 0;
    return m_listen_sockets.front().external_port;
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
        LERR_ << "*** TICK TIMER FAILED " << e.message() << "\n";
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
    error_code ec;
    if (m_settings.send_socket_buffer_size)
    {
        tcp::socket::send_buffer_size option(m_settings.send_socket_buffer_size);
        s.set_option(option, ec);
    }
    if (m_settings.recv_socket_buffer_size)
    {
        tcp::socket::receive_buffer_size option(m_settings.recv_socket_buffer_size);
        s.set_option(option, ec);
    }
}

session_impl::listen_socket_t session_impl::setup_listener(
    tcp::endpoint ep, bool v6_only)
{
    error_code ec;
    listen_socket_t s;
    s.sock.reset(new tcp::acceptor(m_io_service));
    s.sock->open(ep.protocol(), ec);

    if (ec)
    {
        LERR_ << "failed to open socket: " << libtorrent::print_endpoint(ep)
              << ": " << ec.message();
    }

    s.sock->bind(ep, ec);

    if (ec)
    {
        // post alert

        char msg[200];
        snprintf(msg, 200, "cannot bind to interface \"%s\": %s",
                 libtorrent::print_endpoint(ep).c_str(), ec.message().c_str());
        LERR_ << msg;

        return listen_socket_t();
    }

    s.external_port = s.sock->local_endpoint(ec).port();
    s.sock->listen(5, ec);

    if (ec)
    {
        // post alert

        char msg[200];
        snprintf(msg, 200, "cannot listen on interface \"%s\": %s",
                 libtorrent::print_endpoint(ep).c_str(), ec.message().c_str());
        LERR_ << msg;

        return listen_socket_t();
    }

    // post alert succeeded

    LDBG_ << "listening on: " << ep << " external port: " << s.external_port;

    return s;
}
