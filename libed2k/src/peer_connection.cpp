
#include <libtorrent/piece_picker.hpp>
#include <libtorrent/storage.hpp>

#include "peer_connection.hpp"
#include "session_impl.hpp"
#include "session.hpp"
#include "transfer.hpp"
#include "base_socket.hpp"
#include "util.hpp"

using namespace libed2k;
namespace ip = boost::asio::ip;

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
                                 boost::shared_ptr<base_socket> s,
                                 const ip::tcp::endpoint& remote, peer* peerinfo):
    m_ses(ses),
    m_work(ses.m_io_service),
    m_last_receive(time_now()),
    m_last_sent(time_now()),
    m_disk_recv_buffer(ses.m_disk_thread, 0),
    m_socket(s),
    m_remote(remote),
    m_transfer(transfer),
    m_peer_info(peerinfo),
    m_connection_ticket(-1),
    m_connecting(true),
    m_packet_size(0),
    m_recv_pos(0),
    m_disk_recv_buffer_size(0)
{
    m_channel_state[upload_channel] = bw_idle;
    m_channel_state[download_channel] = bw_idle;
    init_handlers();
}

peer_connection::peer_connection(aux::session_impl& ses,
                                 boost::shared_ptr<base_socket> s,
                                 const ip::tcp::endpoint& remote,
                                 peer* peerinfo):
    m_ses(ses),
    m_work(ses.m_io_service),
    m_last_receive(time_now()),
    m_last_sent(time_now()),
    m_disk_recv_buffer(ses.m_disk_thread, 0),
    m_socket(s),
    m_remote(remote),
    m_peer_info(peerinfo),
    m_connection_ticket(-1),
    m_connecting(false),
    m_packet_size(0),
    m_recv_pos(0),
    m_disk_recv_buffer_size(0)
{
    m_channel_state[upload_channel] = bw_idle;
    m_channel_state[download_channel] = bw_idle;
    init_handlers();
}

void peer_connection::init_handlers()
{
    m_socket->set_unhandled_callback(boost::bind(&peer_connection::on_unhandled_packet, self(), _1));   //!< handler for unknown packets
    m_socket->add_callback(OP_HELLO, boost::bind(&peer_connection::on_hello_packet,     self(), _1));
}

peer_connection::~peer_connection()
{
    m_disk_recv_buffer_size = 0;
    DBG("*** PEER CONNECTION CLOSED");
}

void peer_connection::second_tick()
{
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

        m_last_receive = time_now();
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

bool peer_connection::allocate_disk_receive_buffer(int disk_buffer_size)
{
    if (disk_buffer_size == 0) return true;

    if (disk_buffer_size > 16 * 1024)
    {
        disconnect(libtorrent::errors::invalid_piece_size, 2);
        return false;
    }

    // first free the old buffer
    m_disk_recv_buffer.reset();
    // then allocate a new one

    m_disk_recv_buffer.reset(m_ses.allocate_disk_buffer("receive buffer"));
    if (!m_disk_recv_buffer)
    {
        disconnect(libtorrent::errors::no_memory);
        return false;
    }
    m_disk_recv_buffer_size = disk_buffer_size;
    return true;
}

char* peer_connection::release_disk_receive_buffer()
{
    m_disk_recv_buffer_size = 0;
    return m_disk_recv_buffer.release();
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

        disconnect(libtorrent::errors::no_memory);
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
        m_socket->socket().async_write_some(
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
            m_socket->socket().async_read_some(
                boost::asio::mutable_buffers_1(vec[0]), make_read_handler(
                    boost::bind(&peer_connection::on_receive_data, self(), _1, _2)));
        }
        else
        {
            m_socket->socket().async_read_some(
                vec, make_read_handler(
                    boost::bind(&peer_connection::on_receive_data, self(), _1, _2)));
        }
        return 0;
    }

    size_t ret = 0;
    if (num_bufs == 1)
    {
        ret = m_socket->socket().read_some(boost::asio::mutable_buffers_1(vec[0]), ec);
    }
    else
    {
        ret = m_socket->socket().read_some(vec, ec);
    }

    return ret;
}

void peer_connection::on_timeout()
{
    // TODO: implement
}

void peer_connection::disconnect(error_code const& ec, int error)
{
    DBG("peer_connection::disconnect(" << ec.message() << ")" );
}

void peer_connection::on_connect(int ticket)
{
    boost::mutex::scoped_lock l(m_ses.m_mutex);

    m_connection_ticket = ticket;

    boost::shared_ptr<transfer> t = m_transfer.lock();
    error_code ec;

    if (!t)
    {
        disconnect(libtorrent::errors::torrent_aborted);
        return;
    }

    m_socket->socket().open(m_remote.protocol(), ec);
    if (ec)
    {
        disconnect(ec);
        return;
    }

    tcp::endpoint bind_interface = t->get_interface();

    m_socket->socket().set_option(tcp::acceptor::reuse_address(true), ec);
    if (ec)
    {
        disconnect(ec);
        return;
    }

    bind_interface.port(m_ses.listen_port());

    m_socket->socket().bind(bind_interface, ec);
    if (ec)
    {
        disconnect(ec);
        return;
    }

    m_socket->socket().async_connect(
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
    buffer::const_interval recv_buffer = receive_buffer();
    int recv_pos = recv_buffer.end - recv_buffer.begin;
    int header_size = 0; // ???

    if (recv_pos == 1) // receive buffer is empty
    {
        if (!allocate_disk_receive_buffer(m_packet_size - header_size))
        {
            return;
        }
    }

    peer_request p;

    if (recv_pos >= header_size) // read header
    {
        const char* ptr = recv_buffer.begin + 1;
        p.piece = libtorrent::detail::read_int32(ptr);
        p.start = libtorrent::detail::read_int32(ptr);
        p.length = m_packet_size - header_size;
    }

    int piece_bytes = 0;
    if (recv_pos <= header_size)
    {
        // only received protocol data
    }
    else if (recv_pos - received >= header_size)
    {
        // only received payload data
        piece_bytes = received;
    }
    else
    {
        // received a bit of both
        piece_bytes = recv_pos - header_size;
    }

    if (recv_pos < header_size) return;

    if (recv_pos - received < header_size && recv_pos >= header_size)
    {
        // call this once, the first time the entire header
        // has been received
        start_receive_piece(p);
        if (is_disconnecting()) return;
    }

    incoming_piece_fragment(piece_bytes);
    if (!packet_finished()) return;

    disk_buffer_holder holder(m_ses.m_disk_thread, release_disk_receive_buffer());
    incoming_piece(p, holder);
}

void peer_connection::on_cancel(int received)
{
}

void peer_connection::incoming_piece(peer_request const& p, disk_buffer_holder& data)
{
    boost::shared_ptr<transfer> t = m_transfer.lock();

    if (is_disconnecting()) return;
    if (t->is_seed()) return;

    piece_picker& picker = t->picker();
    piece_manager& fs = t->filesystem();
    piece_block block_finished(p.piece, p.start / t->block_size());

    fs.async_write(p, data, boost::bind(&peer_connection::on_disk_write_complete,
                                        self(), _1, _2, p, t));

    bool was_finished = picker.is_piece_finished(p.piece);
    picker.mark_as_writing(block_finished, peer_info());

    // did we just finish the piece?
    // this means all blocks are either written
    // to disk or are in the disk write cache
    if (picker.is_piece_finished(p.piece) && !was_finished)
    {
        t->async_verify_piece(
            p.piece, boost::bind(&transfer::piece_finished, t, p.piece, _1));
    }
}

void peer_connection::incoming_piece_fragment(int bytes)
{
}

void peer_connection::start_receive_piece(peer_request const& r)
{
}

void peer_connection::start()
{
    m_socket->start_read_cycle();
}

bool peer_connection::can_write() const
{
	// TODO - should implement
	return (true);
}

bool peer_connection::can_read(char* state) const
{
    // TODO - should implement
	return (true);  
}

void peer_connection::on_disk_write_complete(
    int ret, disk_io_job const& j, peer_request r, boost::shared_ptr<transfer> t)
{
    boost::mutex::scoped_lock l(m_ses.m_mutex);

    // in case the outstanding bytes just dropped down
    // to allow to receive more data
    setup_receive();

    if (t->is_seed()) return;

    piece_block block_finished(r.piece, r.start / t->block_size());
    piece_picker& picker = t->picker();
    picker.mark_as_finished(block_finished, peer_info());
}

void peer_connection::on_unhandled_packet(const error_code& error)
{
    DBG("unhandled packet received: " << packetToString(m_socket->context().m_type));
}

void peer_connection::on_hello_packet(const error_code& error)
{
    DBG("hello packet received: " << packetToString(m_socket->context().m_type));
    if (!error)
    {
        //TODO - replace this code
        // prepare hello answer
        client_hello_answer cha;
        cha.m_hClient               = m_ses.settings().client_hash;
        cha.m_sNetIdentifier.m_nIP  = 0;
        cha.m_sNetIdentifier.m_nPort= m_ses.settings().listen_port;

        boost::uint32_t nVersion = 0x3c;
        boost::uint32_t nCapability = CAPABLE_AUXPORT | CAPABLE_NEWTAGS | CAPABLE_UNICODE | CAPABLE_LARGEFILES;
        boost::uint32_t nClientVersion  = (3 << 24) | (2 << 17) | (3 << 10) | (1 << 7);
        boost::uint32_t nUdpPort = 0;

        cha.m_tlist.add_tag(make_string_tag(std::string(m_ses.settings().client_name), CT_NAME, true));
        cha.m_tlist.add_tag(make_typed_tag(nVersion, CT_VERSION, true));
        cha.m_tlist.add_tag(make_typed_tag(nUdpPort, CT_EMULE_UDPPORTS, true));
        cha.m_sServerIdentifier.m_nPort = m_ses.settings().server_port;
        cha.m_sServerIdentifier.m_nIP   = m_ses.settings().server_ip;

        m_socket->do_write(cha);
    }
    else
    {
        ERR("hello packet received error " << error.message());
    }
}
