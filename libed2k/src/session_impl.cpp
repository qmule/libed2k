
#include <libtorrent/peer_connection.hpp>

#include "session_impl.hpp"
#include "transfer_handle.hpp"
#include "error_code.hpp"
#include "peer_connection.hpp"

using namespace libed2k;
using namespace libed2k::aux;

session_impl::session_impl(int lst_port, const char* listen_interface,
                           const fingerprint& id, const std::string& logpath):
    m_ipv4_peer_pool(500),
    m_send_buffers(send_buffer_size),
    m_files(40),
    m_io_service(),
	m_host_resolver(m_io_service),
    m_alerts(m_io_service),
    m_disk_thread(m_io_service, boost::bind(&session_impl::on_disk_queue, this),
                  m_files),
    m_half_open(m_io_service),
    m_download_rate(libtorrent::peer_connection::download_channel),
    m_upload_rate(libtorrent::peer_connection::upload_channel),
    m_server_manager(*this),
    m_abort(false),
    m_paused(false),
    m_max_uploads(8),
    m_allowed_upload_slots(8),
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
    DWORD windows_version = ::GetVersion();
    if ((windows_version & 0xff) >= 6)
    {
        // on vista the limit is 5 (in home edition)
        m_half_open.limit(4);
    }
    else
    {
        // on XP SP2 it's 10
        m_half_open.limit(8);
    }
#endif

    m_bandwidth_channel[libtorrent::peer_connection::download_channel] =
        &m_download_channel;
    m_bandwidth_channel[libtorrent::peer_connection::upload_channel] =
        &m_upload_channel;

    m_logger = create_log("main_session", listen_port(), false);
    (*m_logger) << libtorrent::time_now_string() << "\n";

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

    if (m_ip_filter.access(endp.address()) & ip_filter::blocked)
    {
        (*m_logger) << "filtered blocked ip\n";

        if (m_alerts.should_post<peer_blocked_alert>())
            m_alerts.post_alert(peer_blocked_alert(endp.address()));

        return;
    }

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

    boost::intrusive_ptr<peer_connection> c(
        new peer_connection(*this, s, endp));

    if (!c->is_disconnecting())
    {
        m_connections.insert(c);
        c->start();
    }
}

unsigned short session_impl::listen_port() const
{
}

void session_impl::on_disk_queue()
{
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
