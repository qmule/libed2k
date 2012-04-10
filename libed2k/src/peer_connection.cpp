
#include "peer_connection.hpp"
#include "session_impl.hpp"
#include "session.hpp"
#include "transfer.hpp"

using namespace libed2k;

peer_connection::peer_connection(aux::session_impl& ses,
                                 boost::weak_ptr<transfer> transfer,
                                 boost::shared_ptr<tcp::socket> s,
                                 const tcp::endpoint& remote, peer* peerinfo):
    m_ses(ses), m_socket(s), m_remote(remote), m_transfer(transfer),
    m_connection_ticket(-1), m_connecting(true)
{
    m_channel_state[upload_channel] = bw_idle;
    m_channel_state[download_channel] = bw_idle;
}

peer_connection::peer_connection(aux::session_impl& ses,
                                 boost::shared_ptr<tcp::socket> s,
                                 const tcp::endpoint& remote,
                                 peer* peerinfo):
    m_ses(ses), m_socket(s), m_remote(remote), m_connection_ticket(-1),
    m_connecting(false)
{
    m_channel_state[upload_channel] = bw_idle;
    m_channel_state[download_channel] = bw_idle;
}

peer_connection::~peer_connection()
{
    // TODO: implement
}


void peer_connection::on_send_data(error_code const& error,
                                   std::size_t bytes_transferred)
{
}

void peer_connection::on_receive_data(error_code const& error,
                                      std::size_t bytes_transferred)
{
}

void peer_connection::on_receive_data_nolock(
    error_code const& error, std::size_t bytes_transferred)
{
}

void peer_connection::send_buffer(char const* buf, int size, int flags)
{
    if (flags == message_type_request)
        m_requests_in_buffer.push_back(m_send_buffer.size() + size);

    int free_space = m_send_buffer.space_in_last_buffer();
    if (free_space > size) free_space = size;
    if (free_space > 0)
    {
        m_send_buffer.append(buf, free_space);
        size -= free_space;
        buf += free_space;
    }
    if (size <= 0) return;

    std::pair<char*, int> buffer = m_ses.allocate_buffer(size);
    if (buffer.first == 0)
    {
        disconnect(errors::no_memory);
        return;
    }
    TORRENT_ASSERT(buffer.second >= size);
    std::memcpy(buffer.first, buf, size);
    m_send_buffer.append_buffer(buffer.first, buffer.second, size,
                                boost::bind(&aux::session_impl::free_buffer,
                                            boost::ref(m_ses), _1, buffer.second));

    setup_send();
}

void peer_connection::setup_send()
{
    if (m_channel_state[upload_channel] != bw_idle) return;

    if (!can_write())
    {
        return;
    }

    // send the actual buffer
    if (!m_send_buffer.empty())
    {
        // check quota here
        int amount_to_send = m_send_buffer.size();

        std::list<boost::asio::const_buffer> const& vec =
            m_send_buffer.build_iovec(amount_to_send);
        m_socket->async_write_some(
            vec, make_write_handler(boost::bind(&peer_connection::on_send_data,
                                                self(), _1, _2)));
    }

    m_channel_state[upload_channel] = bw_network;
}

void peer_connection::setup_receive(sync_t sync)
{
    if (m_channel_state[download_channel] != bw_idle
        && m_channel_state[download_channel] != bw_disk) return;

    // check quota here

    if (!can_read(&m_channel_state[download_channel]))
    {
        // if we block reading, waiting for the disk, we will wake up
        // by the disk_io_thread posting a message every time it drops
        // from being at or exceeding the limit down to below the limit

        return;
    }

    error_code ec;

    if (sync == read_sync)
    {
        size_t bytes_transferred = try_read(read_sync, ec);

        if (ec != boost::asio::error::would_block)
        {
            m_channel_state[download_channel] = bw_network;
            on_receive_data_nolock(ec, bytes_transferred);

            return;
        }
    }

    try_read(read_async, ec);
}

size_t peer_connection::try_read(sync_t s, error_code& ec)
{
}

void peer_connection::on_timeout()
{
    // TODO: implement
}

void peer_connection::disconnect(error_code const& ec, int error)
{
    // TODO: implement
}

void peer_connection::on_connect(int ticket)
{
    boost::mutex::scoped_lock l(m_ses.m_mutex);

    m_connection_ticket = ticket;

    boost::shared_ptr<transfer> t = m_transfer.lock();
    error_code ec;

    if (!t)
    {
        disconnect(errors::torrent_aborted);
        return;
    }

    m_socket->open(m_remote.protocol(), ec);
    if (ec)
    {
        disconnect(ec);
        return;
    }

    // set the socket to non-blocking, so that we can
    // read the entire buffer on each read event we get
    tcp::socket::non_blocking_io ioc(true);
    m_socket->io_control(ioc, ec);
    if (ec)
    {
        disconnect(ec);
        return;
    }

    tcp::endpoint bind_interface = t->get_interface();

    m_socket->set_option(tcp::acceptor::reuse_address(true), ec);
    if (ec)
    {
        disconnect(ec);
        return;
    }

    bind_interface.port(m_ses.port());

    m_socket->bind(bind_interface, ec);
    if (ec)
    {
        disconnect(ec);
        return;
    }

    m_socket->async_connect(
        m_remote, boost::bind(&peer_connection::on_connection_complete, self(), _1));

}

void peer_connection::on_connection_complete(error_code const& e)
{
    boost::mutex::scoped_lock l(m_ses.m_mutex);

    if (m_disconnecting) return;

    m_connecting = false;
    m_ses.m_half_open.done(m_connection_ticket);

    error_code ec;
    if (e)
    {
        disconnect(e, 1);
        return;
    }

    if (m_disconnecting) return;

    setup_send();
    setup_receive();

}

void peer_connection::start()
{
}

bool peer_connection::can_write() const
{
}

bool peer_connection::can_read(char* state) const
{
}
