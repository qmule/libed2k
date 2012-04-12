
#include "peer_connection.hpp"
#include "session_impl.hpp"
#include "session.hpp"
#include "transfer.hpp"

using namespace libed2k;

int libed2k::round_up8(int v)
{
    return ((v & 7) == 0) ? v : v + (8 - (v & 7));
}

const libed2k::peer_connection::message_handler
libed2k::peer_connection::m_message_handler[] =
{
    &peer_connection::on_interested,
    &peer_connection::on_not_interested,
    &peer_connection::on_have,
    &peer_connection::on_request,
    &peer_connection::on_piece,
    &peer_connection::on_cancel
};

peer_connection::peer_connection(aux::session_impl& ses,
                                 boost::weak_ptr<transfer> transfer,
                                 boost::shared_ptr<tcp::socket> s,
                                 const tcp::endpoint& remote, peer* peerinfo):
    m_ses(ses),
    m_work(ses.m_io_service),
    m_last_receive(libtorrent::time_now()),
    m_last_sent(libtorrent::time_now()),
    m_disk_recv_buffer(ses.m_disk_thread, 0),
    m_socket(s),
    m_remote(remote),
    m_transfer(transfer),
    m_connection_ticket(-1),
    m_connecting(true),
    m_packet_size(0),
    m_recv_pos(0),
    m_disk_recv_buffer_size(0)
{
    m_channel_state[upload_channel] = bw_idle;
    m_channel_state[download_channel] = bw_idle;
}

peer_connection::peer_connection(aux::session_impl& ses,
                                 boost::shared_ptr<tcp::socket> s,
                                 const tcp::endpoint& remote,
                                 peer* peerinfo):
    m_ses(ses),
    m_work(ses.m_io_service),
    m_last_receive(libtorrent::time_now()),
    m_last_sent(libtorrent::time_now()),
    m_disk_recv_buffer(ses.m_disk_thread, 0),
    m_socket(s),
    m_remote(remote),
    m_connection_ticket(-1),
    m_connecting(false),
    m_packet_size(0),
    m_recv_pos(0),
    m_disk_recv_buffer_size(0)
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
    boost::mutex::scoped_lock l(m_ses.m_mutex);
    on_receive_data_nolock(error, bytes_transferred);
}

void peer_connection::on_receive_data_nolock(
    error_code const& error, std::size_t bytes_transferred)
{
    // keep ourselves alive in until this function exits in
    // case we disconnect
    boost::intrusive_ptr<peer_connection> me(self());

    m_channel_state[download_channel] = bw_idle;

    int bytes_in_loop = bytes_transferred;

    if (error)
    {
        on_receive(error, bytes_transferred);
        disconnect(error);
        return;
    }

    int num_loops = 0;
    do
    {
        // correct the dl quota usage, if not all of the buffer was actually read
        //m_quota[download_channel] -= bytes_transferred;

        if (m_disconnecting)
        {
            return;
        }

        m_last_receive = libtorrent::time_now();
        m_recv_pos += bytes_transferred;

        on_receive(error, bytes_transferred);

        if (m_disconnecting) return;

        //if (m_recv_pos >= m_soft_packet_size) m_soft_packet_size = 0;

        if (num_loops > 20) break;

        error_code ec;
        bytes_transferred = try_read(read_sync, ec);
        if (ec && ec != boost::asio::error::would_block)
        {
            disconnect(ec);
            return;
        }

        if (ec == boost::asio::error::would_block) break;
        bytes_in_loop += bytes_transferred;
        ++num_loops;
    }
    while (bytes_transferred > 0);

    setup_receive(read_async);
}

void peer_connection::on_sent(error_code const& error, std::size_t bytes_transferred)
{
}

void peer_connection::on_receive(error_code const& error, std::size_t bytes_transferred)
{
    if (error) return;

    //buffer::const_interval recv_buffer = receive_buffer();

    if (m_state == read_packet)
    {
        dispatch_message(bytes_transferred);
        return;
    }

}

bool peer_connection::dispatch_message(int received)
{
    buffer::const_interval recv_buffer = receive_buffer();

    int packet_type = (unsigned char)recv_buffer[0];

    // call the correct handler for this packet type
    (this->*m_message_handler[packet_type])(received);

    return packet_finished();
}

void peer_connection::reset_recv_buffer(int packet_size)
{
    if (m_recv_pos > m_packet_size)
    {
        cut_receive_buffer(m_packet_size, packet_size);
        return;
    }
    m_recv_pos = 0;
    m_packet_size = packet_size;
}

void peer_connection::cut_receive_buffer(int size, int packet_size)
{
    if (size > 0)
        std::memmove(&m_recv_buffer[0], &m_recv_buffer[0] + size, m_recv_pos - size);

    m_recv_pos -= size;
    m_packet_size = packet_size;
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
    int max_receive = m_packet_size - m_recv_pos;

    // check quota here

    if (max_receive == 0 || !can_read())
    {
        ec = boost::asio::error::would_block;
        return 0;
    }

    int regular_buffer_size = m_packet_size - m_disk_recv_buffer_size;

    if (int(m_recv_buffer.size()) < regular_buffer_size)
        m_recv_buffer.resize(round_up8(regular_buffer_size));

    boost::array<boost::asio::mutable_buffer, 2> vec;
    int num_bufs = 0;
    if (!m_disk_recv_buffer || regular_buffer_size >= m_recv_pos + max_receive)
    {
        // only receive into regular buffer
        vec[0] = boost::asio::buffer(&m_recv_buffer[m_recv_pos], max_receive);
        num_bufs = 1;
    }
    else if (m_recv_pos >= regular_buffer_size)
    {
        // only receive into disk buffer
        vec[0] = boost::asio::buffer(
            m_disk_recv_buffer.get() + m_recv_pos - regular_buffer_size, max_receive);
        num_bufs = 1;
    }
    else
    {
        // receive into both regular and disk buffer
        vec[0] = boost::asio::buffer(&m_recv_buffer[m_recv_pos],
                                     regular_buffer_size - m_recv_pos);
        vec[1] = boost::asio::buffer(m_disk_recv_buffer.get(),
                                     max_receive - regular_buffer_size + m_recv_pos);
        num_bufs = 2;
    }

    if (s == read_async)
    {
        m_channel_state[download_channel] = bw_network;

        if (num_bufs == 1)
        {
            m_socket->async_read_some(
                boost::asio::mutable_buffers_1(vec[0]), make_read_handler(
                    boost::bind(&peer_connection::on_receive_data, self(), _1, _2)));
        }
        else
        {
            m_socket->async_read_some(
                vec, make_read_handler(
                    boost::bind(&peer_connection::on_receive_data, self(), _1, _2)));
        }
        return 0;
    }

    size_t ret = 0;
    if (num_bufs == 1)
    {
        ret = m_socket->read_some(boost::asio::mutable_buffers_1(vec[0]), ec);
    }
    else
    {
        ret = m_socket->read_some(vec, ec);
    }

    return ret;
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

void peer_connection::on_interested(int received)
{
}

void peer_connection::on_not_interested(int received)
{
}

void peer_connection::on_have(int received)
{
}

void peer_connection::on_request(int received)
{
}

// -----------------------------
// ----------- PIECE -----------
// -----------------------------
void peer_connection::on_piece(int received)
{
}

void peer_connection::on_cancel(int received)
{
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
