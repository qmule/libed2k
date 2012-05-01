
#include <libtorrent/piece_picker.hpp>
#include <libtorrent/storage.hpp>

#include "peer_connection.hpp"
#include "session_impl.hpp"
#include "session.hpp"
#include "transfer.hpp"
#include "file.hpp"
#include "util.hpp"

using namespace libed2k;
namespace ip = boost::asio::ip;

peer_connection::peer_connection(aux::session_impl& ses,
                                 boost::weak_ptr<transfer> transfer,
                                 boost::shared_ptr<tcp::socket> s,
                                 const ip::tcp::endpoint& remote, peer* peerinfo):
    base_connection(ses, s, remote),
    m_work(ses.m_io_service),
    m_last_receive(time_now()),
    m_last_sent(time_now()),
    m_disk_recv_buffer(ses.m_disk_thread, 0),
    m_transfer(transfer),
    m_peer_info(peerinfo),
    m_connecting(true),
    m_packet_size(0),
    m_recv_pos(0),
    m_disk_recv_buffer_size(0)
{
    init();
}

peer_connection::peer_connection(aux::session_impl& ses,
                                 boost::shared_ptr<tcp::socket> s,
                                 const ip::tcp::endpoint& remote,
                                 peer* peerinfo):
    base_connection(ses, s, remote),
    m_work(ses.m_io_service),
    m_last_receive(time_now()),
    m_last_sent(time_now()),
    m_disk_recv_buffer(ses.m_disk_thread, 0),
    m_peer_info(peerinfo),
    m_connecting(false),
    m_packet_size(0),
    m_recv_pos(0),
    m_disk_recv_buffer_size(0)
{
    init();
}

void peer_connection::init()
{
    m_disconnecting = false;
    m_connection_ticket = -1;

    // hello handler
    add_handler(OP_HELLO, boost::bind(
                    &peer_connection::on_hello, this, _1));
    // hello answer handler
    add_handler(OP_HELLOANSWER, boost::bind(
                    &peer_connection::on_hello_answer, this, _1));
}

peer_connection::~peer_connection()
{
    m_disk_recv_buffer_size = 0;
    DBG("*** PEER CONNECTION CLOSED");
}

void peer_connection::second_tick()
{
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

void peer_connection::on_timeout()
{
    boost::mutex::scoped_lock l(m_ses.m_mutex);

    error_code ec;
    DBG("CONNECTION TIMED OUT: " << m_remote);

    disconnect(errors::timed_out, 1);
}

void peer_connection::disconnect(error_code const& ec, int error)
{
    switch (error)
    {
    case 0:
        DBG("*** CONNECTION CLOSED " << ec.message());
        break;
    case 1:
        DBG("*** CONNECTION FAILED " << ec.message());
        break;
    case 2:
        DBG("*** PEER ERROR " << ec.message());
        break;
    }
    // TODO: implement

    //if (error > 0) m_failed = true;
    if (m_disconnecting)
    {
        DBG("peer_connection::disconnect: return because disconnecting");
        return;
    }

    boost::intrusive_ptr<peer_connection> me(this);

    if (m_connecting && m_connection_ticket >= 0)
    {
        m_ses.m_half_open.done(m_connection_ticket);
        m_connection_ticket = -1;
    }

    boost::shared_ptr<transfer> t = m_transfer.lock();

    if (t)
    {
        if (t->has_picker())
        {
            piece_picker& picker = t->picker();

            while (!m_download_queue.empty())
            {
                pending_block& qe = m_download_queue.back();
                if (!qe.timed_out && !qe.not_wanted) picker.abort_download(qe.block);
                //m_outstanding_bytes -= t->to_req(qe.block).length;
                //if (m_outstanding_bytes < 0) m_outstanding_bytes = 0;
                m_download_queue.pop_back();
            }
            while (!m_request_queue.empty())
            {
                picker.abort_download(m_request_queue.back().block);
                m_request_queue.pop_back();
            }
        }
        else
        {
            m_download_queue.clear();
            m_request_queue.clear();
            //m_outstanding_bytes = 0;
        }
        //m_queued_time_critical = 0;

        t->remove_peer(this);
        m_transfer.reset();
    }

    DBG("peer_connection::disconnect: close");
    m_disconnecting = true;
    close(ec);
    m_ses.close_connection(this, ec);
}

void peer_connection::connect(int ticket)
{
    boost::mutex::scoped_lock l(m_ses.m_mutex);

    m_connection_ticket = ticket;

    boost::shared_ptr<transfer> t = m_transfer.lock();
    error_code ec;

    DBG("CONNECTING: " << m_remote);

    if (!t)
    {
        disconnect(errors::transfer_aborted);
        return;
    }

    m_socket->async_connect(
        m_remote, boost::bind(&peer_connection::on_connect,
                              self_as<peer_connection>(), _1));

}

void peer_connection::on_connect(error_code const& e)
{
    boost::mutex::scoped_lock l(m_ses.m_mutex);

    if (m_disconnecting) return;

    m_connecting = false;
    m_ses.m_half_open.done(m_connection_ticket);

    error_code ec;
    if (e)
    {
        DBG("CONNECTION FAILED: " << m_remote << ": " << e.message());

        disconnect(e, 1);
        return;
    }

    if (m_disconnecting) return;

    DBG("COMPLETED: " << m_remote);

    // connection is already established
    do_read();
    write_hello();
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

void peer_connection::incoming_piece(peer_request const& p, disk_buffer_holder& data)
{
    boost::shared_ptr<transfer> t = m_transfer.lock();

    if (is_disconnecting()) return;
    if (t->is_seed()) return;

    piece_picker& picker = t->picker();
    piece_manager& fs = t->filesystem();
    piece_block block_finished(p.piece, p.start / t->block_size());

    fs.async_write(p, data, boost::bind(&peer_connection::on_disk_write_complete,
                                        self_as<peer_connection>(), _1, _2, p, t));

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
    // REALLY? : connection is already established
    do_read();
}

void peer_connection::on_error(const error_code& error)
{
    disconnect(error);
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
    do_read();

    if (t->is_seed()) return;

    piece_block block_finished(r.piece, r.start / t->block_size());
    piece_picker& picker = t->picker();
    picker.mark_as_finished(block_finished, peer_info());
}

void peer_connection::on_hello(const error_code& error)
{
    DBG("hello packet <== " << m_remote);
    if (!error)
    {
        write_hello_answer();
    }
    else
    {
        ERR("hello packet received error " << error.message());
    }
}

void peer_connection::on_hello_answer(const error_code& error)
{
    DBG("hello answer <== " << m_remote);
    if (!error)
    {
    }
    else
    {
        ERR("hello answer received error " << error.message());
    }
}

void peer_connection::write_hello()
{
    DBG("hello ==> " << m_remote);

    const session_settings& settings = m_ses.settings();
    boost::uint32_t nVersion = 0x3c;
    client_hello hello;
    hello.m_nHashLength = MD4_HASH_SIZE;
    hello.m_hClient = settings.client_hash;
    hello.m_sNetIdentifier.m_nIP = m_ses.m_client_id;
    hello.m_sNetIdentifier.m_nPort = settings.listen_port;
    hello.m_tlist.add_tag(make_string_tag(settings.client_name, CT_NAME, true));
    hello.m_tlist.add_tag(make_typed_tag(nVersion, CT_VERSION, true));

    do_write(hello);
}

void peer_connection::write_hello_answer()
{
    DBG("hello answer ==> " << m_remote);
    //TODO - replace this code
    // prepare hello answer
    client_hello_answer cha;
    cha.m_hClient               = m_ses.settings().client_hash;
    cha.m_sNetIdentifier.m_nIP  = 0;
    cha.m_sNetIdentifier.m_nPort= m_ses.settings().listen_port;

    boost::uint32_t nVersion = 0x3c;
    //boost::uint32_t nCapability = CAPABLE_AUXPORT | CAPABLE_NEWTAGS | 
    //    CAPABLE_UNICODE | CAPABLE_LARGEFILES;
    //boost::uint32_t nClientVersion  = (3 << 24) | (2 << 17) | (3 << 10) | (1 << 7);
    boost::uint32_t nUdpPort = 0;

    cha.m_tlist.add_tag(
        make_string_tag(std::string(m_ses.settings().client_name), CT_NAME, true));
    cha.m_tlist.add_tag(make_typed_tag(nVersion, CT_VERSION, true));
    cha.m_tlist.add_tag(make_typed_tag(nUdpPort, CT_EMULE_UDPPORTS, true));
    cha.m_sServerIdentifier.m_nPort = m_ses.settings().server_port;
    cha.m_sServerIdentifier.m_nIP   = m_ses.settings().server_ip;

    do_write(cha);
}

void peer_connection::write_file_request(const md4_hash& file_hash)
{
    client_file_request fr;
    fr.m_hFile = file_hash;
    do_write(fr);
}

void peer_connection::write_no_file(const md4_hash& file_hash)
{
    client_no_file nf;
    nf.m_hFile = file_hash;
    do_write(nf);
}

void peer_connection::write_file_status(
    const md4_hash& file_hash, const bitfield& status)
{
    client_file_status fs;
    fs.m_hFile = file_hash;
    fs.m_vcStatus.m_collection.assign(
        status.bytes(), status.bytes() + bits2bytes(status.size()));
    do_write(fs);
}

void peer_connection::write_hashset_request(const md4_hash& file_hash)
{
    client_hashset_request hr;
    hr.m_hFile = file_hash;
    do_write(hr);
}

void peer_connection::write_hashset_answer(const known_file& file)
{
    client_hashset_answer ha;
    ha.m_hFile = file.getFileHash();
    for (size_t part = 0; part < file.getPiecesCount(); ++part)
        ha.m_vhParts.m_collection.push_back(file.getPieceHash(part));
    do_write(ha);
}

void peer_connection::write_start_upload(const md4_hash& file_hash)
{
    client_start_upload su;
    su.m_hFile = file_hash;
    do_write(su);
}

void peer_connection::write_queue_ranking(boost::uint16_t rank)
{
    client_queue_ranking qr;
    qr.m_nRank = rank;
    do_write(qr);
}

void peer_connection::write_accept_upload()
{
    client_accept_upload au;
    do_write(au);
}

void peer_connection::write_cancel_transfer()
{
    client_cancel_transfer ct;
    do_write(ct);
}
