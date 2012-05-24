
#include <libtorrent/piece_picker.hpp>
#include <libtorrent/storage.hpp>

#include "libed2k/peer_connection.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/session.hpp"
#include "libed2k/transfer.hpp"
#include "libed2k/file.hpp"
#include "libed2k/util.hpp"
#include "libed2k/alert_types.hpp"

using namespace libed2k;
namespace ip = boost::asio::ip;

#define DECODE_PACKET(packet_struct)\
        packet_struct packet;\
        if (!decode_packet(packet))\
        {\
            close(errors::decode_packet_error);\
        }


peer_request mk_peer_request(size_t begin, size_t end)
{
    peer_request r;
    r.piece = begin / PIECE_SIZE;
    r.start = begin % PIECE_SIZE;
    r.length = end - begin;
    return r;
}

std::pair<peer_request, peer_request> split_request(const peer_request& req)
{
    peer_request r = req;
    peer_request left = req;
    r.length = std::min(req.length, int(DISK_BLOCK_SIZE));
    left.start = r.start + r.length;
    left.length = req.length - r.length;

    return std::make_pair(r, left);
}

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
    m_peer(peerinfo),
    m_connecting(true),
    m_active(true),
    m_disk_recv_buffer_size(0)
{
    reset();
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
    m_peer(peerinfo),
    m_connecting(false),
    m_active(false),
    m_disk_recv_buffer_size(0)
{
    reset();

    // connection is already established
    do_read();
}

void peer_connection::reset()
{
    m_disconnecting = false;
    m_connection_ticket = -1;
    m_desired_queue_size = 3;

    m_channel_state[upload_channel] = bw_idle;
    m_channel_state[download_channel] = bw_idle;

    add_handler(OP_HELLO, boost::bind(&peer_connection::on_hello, this, _1));
    add_handler(OP_HELLOANSWER, boost::bind(&peer_connection::on_hello_answer, this, _1));
    add_handler(OP_REQUESTFILENAME, boost::bind(&peer_connection::on_file_request, this, _1));
    add_handler(OP_REQFILENAMEANSWER, boost::bind(&peer_connection::on_file_answer, this, _1));
    add_handler(OP_FILEDESC, boost::bind(&peer_connection::on_file_description, this, _1));
    add_handler(OP_SETREQFILEID, boost::bind(&peer_connection::on_filestatus_request, this, _1));
    add_handler(OP_FILEREQANSNOFIL, boost::bind(&peer_connection::on_no_file, this, _1));
    add_handler(OP_FILESTATUS, boost::bind(&peer_connection::on_file_status, this, _1));
    add_handler(OP_HASHSETREQUEST, boost::bind(&peer_connection::on_hashset_request, this, _1));
    add_handler(OP_HASHSETANSWER, boost::bind(&peer_connection::on_hashset_answer, this, _1));
    add_handler(OP_STARTUPLOADREQ, boost::bind(&peer_connection::on_start_upload, this, _1));
    add_handler(OP_QUEUERANKING, boost::bind(&peer_connection::on_queue_ranking, this, _1));
    add_handler(OP_ACCEPTUPLOADREQ, boost::bind(&peer_connection::on_accept_upload, this, _1));
    add_handler(OP_OUTOFPARTREQS, boost::bind(&peer_connection::on_out_parts, this, _1));
    add_handler(OP_CANCELTRANSFER, boost::bind(&peer_connection::on_cancel_transfer, this, _1));
    add_handler(OP_REQUESTPARTS_I64, boost::bind(&peer_connection::on_request_parts, this, _1));
    add_handler(OP_SENDINGPART,
                boost::bind(&peer_connection::on_sending_part<client_sending_part_32>, this, _1));
    add_handler(OP_SENDINGPART_I64,
                boost::bind(&peer_connection::on_sending_part<client_sending_part_64>, this, _1));
    add_handler(OP_END_OF_DOWNLOAD, boost::bind(&peer_connection::on_end_download, this, _1));
    add_handler(OP_ASKSHAREDFILESANSWER, boost::bind(&peer_connection::on_shared_files_answer, this, _1));

    // clients talking
    add_handler(OP_MESSAGE, boost::bind(&peer_connection::on_client_message, this, _1));
    add_handler(OP_CHATCAPTCHAREQ, boost::bind(&peer_connection::on_client_captcha_request, this, _1));
    add_handler(OP_CHATCAPTCHARES, boost::bind(&peer_connection::on_client_captcha_result, this, _1));
}

peer_connection::~peer_connection()
{
    m_disk_recv_buffer_size = 0;
    DBG("*** PEER CONNECTION CLOSED");
}

void peer_connection::init()
{
}

void peer_connection::second_tick()
{
}

bool peer_connection::attach_to_transfer(const md4_hash& hash)
{
    boost::weak_ptr<transfer> wpt = m_ses.find_transfer(hash);
    boost::shared_ptr<transfer> t = wpt.lock();

    if (t && t->is_aborted())
    {
        DBG("the transfer has been aborted");
        t.reset();
    }

    if (!t)
    {
        // we couldn't find the transfer!
        DBG("couldn't find a transfer with the given hash: " << hash);
        return false;
    }

    if (t->is_paused())
    {
        // paused transfer will not accept incoming connections
        DBG("rejected connection to paused torrent");
        return false;
    }

    // check to make sure we don't have another connection with the same
    // hash and peer_id. If we do. close this connection.
    if (!t->attach_peer(this)) return false;
    m_transfer = wpt;

    // if the transfer isn't ready to accept
    // connections yet, we'll have to wait with
    // our initialization
    if (t->ready_for_connections()) init();
    return true;
}

void peer_connection::request_block()
{
    boost::shared_ptr<transfer> t = m_transfer.lock();

    if (t->is_seed()) return;
    // don't request pieces before we have the metadata
    //if (!t->has_metadata()) return;

    int num_requests = m_desired_queue_size
        - (int)m_download_queue.size()
        - (int)m_request_queue.size();

    // if our request queue is already full, we
    // don't have to make any new requests yet
    if (num_requests <= 0) return;

    piece_picker& p = t->picker();
    std::vector<piece_block> interesting_pieces;
    interesting_pieces.reserve(100);

    int prefer_whole_pieces = 1;

    // TODO: state = c.peer_speed()
    piece_picker::piece_state_t state = piece_picker::medium;

    p.pick_pieces(m_remote_hashset.pieces(), interesting_pieces,
                  num_requests, prefer_whole_pieces, m_peer,
                  state, picker_options(), std::vector<int>());

    for (std::vector<piece_block>::iterator i = interesting_pieces.begin();
         i != interesting_pieces.end(); ++i)
    {
        int num_block_requests = p.num_peers(*i);
        if (num_block_requests > 0)
        {
            continue;
        }

        // don't request pieces we already have in our request queue
        // This happens when pieces time out or the peer sends us
        // pieces we didn't request. Those aren't marked in the
        // piece picker, but we still keep track of them in the
        // download queue
        if (std::find_if(m_download_queue.begin(), m_download_queue.end(),
                         has_block(*i)) != m_download_queue.end() ||
            std::find_if(m_request_queue.begin(), m_request_queue.end(),
                         has_block(*i)) != m_request_queue.end())
        {
            continue;
        }

        // ok, we found a piece that's not being downloaded
        // by somebody else. request it from this peer
        // and return
        if (!add_request(*i, 0)) continue;
        num_requests--;
    }
}

bool peer_connection::add_request(const piece_block& block, int flags)
{
    boost::shared_ptr<transfer> t = m_transfer.lock();

    //if (t->upload_mode()) return false;
    if (m_disconnecting) return false;

    piece_picker::piece_state_t state = piece_picker::medium;

    if (!t->picker().mark_as_downloading(block, get_peer(), state))
        return false;

    //if (t->alerts().should_post<block_downloading_alert>())
    //{
    //    t->alerts().post_alert(
    //        block_downloading_alert(t->get_handle(), remote(), pid(), speedmsg,
    //                                block.block_index, block.piece_index));
    //}

    pending_block pb(block);
    //pb.busy = flags & req_busy;
    m_request_queue.push_back(pb);
    return true;
}

void peer_connection::send_block_requests()
{
    boost::shared_ptr<transfer> t = m_transfer.lock();

    if (m_disconnecting) return;
    if (m_download_queue.size() >= m_desired_queue_size) return;

    client_request_parts_64 rp;
    rp.m_hFile = t->hash();

    while (!m_request_queue.empty() && m_download_queue.size() < m_desired_queue_size)
    {
        pending_block block = m_request_queue.front();
        m_request_queue.erase(m_request_queue.begin());

        // if we're a seed, we don't have a piece picker
        // so we don't have to worry about invariants getting
        // out of sync with it
        if (t->is_seed()) continue;

        // this can happen if a block times out, is re-requested and
        // then arrives "unexpectedly"
        if (t->picker().is_finished(block.block) || t->picker().is_downloaded(block.block))
        {
            t->picker().abort_download(block.block);
            continue;
        }

        m_download_queue.push_back(block);
        rp.append(block_range(block.block.piece_index, block.block.block_index, t->filesize()));
        if (rp.full())
        {
            write_request_parts(rp);
            rp.reset();
        }
    }

    if (!rp.empty())
        write_request_parts(rp);
}

bool peer_connection::allocate_disk_receive_buffer(int disk_buffer_size)
{
    if (disk_buffer_size == 0) return true;

    // first free the old buffer
    m_disk_recv_buffer.reset();
    // then allocate a new one

    m_disk_recv_buffer.reset(m_ses.allocate_disk_buffer("receive buffer"));
    if (!m_disk_recv_buffer)
    {
        disconnect(errors::no_memory);
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
        DBG("peer_connection::disconnect(*** CONNECTION CLOSED) " << ec.message());
        break;
    case 1:
        DBG("peer_connection::disconnect*** CONNECTION FAILED) " << ec.message());
        break;
    case 2:
        DBG("peer_connection::disconnect*** PEER ERROR) " << ec.message());
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
    base_connection::close(ec); // close transport
    m_ses.close_connection(this, ec);
}

void peer_connection::connect(int ticket)
{
    boost::mutex::scoped_lock l(m_ses.m_mutex);

    m_connection_ticket = ticket;

    DBG("CONNECTING: " << m_remote);

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

void peer_connection::start()
{
}

int peer_connection::picker_options() const
{
    int ret = 0;
    boost::shared_ptr<transfer> t = m_transfer.lock();

    if (!t) return 0;

    if (t->is_sequential_download())
    {
        ret |= piece_picker::sequential | piece_picker::ignore_whole_pieces;
    }

    ret |= piece_picker::rarest_first | piece_picker::speed_affinity;

    return ret;
}

void peer_connection::on_error(const error_code& error)
{
    disconnect(error);
}

void peer_connection::close(const error_code& ec)
{
    DBG("peer_connection::close(" << ec << ")");
    disconnect(ec);
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

bool peer_connection::has_ip_address(client_id_type nIP) const
{
    DBG("peer_connection::has_ip_address(" << m_remote.address().to_string());
    return (address2int(m_remote.address()) == nIP);
}

void peer_connection::send_message(const std::string& strMessage)
{
    DBG("peer_connection::send_message()");

    m_messages_order.push_back(client_meta_packet(client_message(strMessage)));

    if (!is_closed())
    {
        fill_send_buffer();
    }
}

void peer_connection::request_shared_files()
{
    m_messages_order.push_back(client_meta_packet(client_shared_files_request()));

    if (!is_closed())
    {
        fill_send_buffer();
    }
}

void peer_connection::fill_send_buffer()
{
    if (m_channel_state[upload_channel] != bw_idle) return;

    int buffer_size_watermark = 512;

    if (!m_messages_order.empty())
    {
        // temp code for testing

        switch(m_messages_order.front().m_proto)
        {
            case OP_MESSAGE:
                 DBG("peer_connection::write message: " << m_messages_order.front().m_message.m_strMessage);
                 do_write(m_messages_order.front().m_message);
                 break;
            case OP_ASKSHAREDFILES:
                DBG("peer_connection::write message: shared_file_list");
                do_write(m_messages_order.front().m_files_request);
                break;
            default:
                break;

        }

        m_messages_order.pop_front();
    }

    if (!m_requests.empty() && m_send_buffer.size() < buffer_size_watermark)
    {
        const peer_request& req = m_requests.front();
        write_part(req);
        send_data(req);
        m_requests.erase(m_requests.begin());
    }
}

void peer_connection::send_data(const peer_request& req)
{
    boost::shared_ptr<transfer> t = m_transfer.lock();
    if (!t) return;

    std::pair<peer_request, peer_request> reqs = split_request(req);
    peer_request r = reqs.first;
    peer_request left = reqs.second;

    if (r.length > 0)
    {
        t->filesystem().async_read(r, boost::bind(&peer_connection::on_disk_read_complete,
                                                  self_as<peer_connection>(), _1, _2, r, left));
        m_channel_state[upload_channel] = bw_network;
    }
    else
    {
        m_channel_state[upload_channel] = bw_idle;
    }
}

void peer_connection::on_disk_read_complete(
    int ret, disk_io_job const& j, peer_request r, peer_request left)
{
    boost::mutex::scoped_lock l(m_ses.m_mutex);

    assert(r.piece == j.piece);
    assert(r.start == j.offset);

    disk_buffer_holder buffer(m_ses.m_disk_thread, j.buffer);
    boost::shared_ptr<transfer> t = m_transfer.lock();

    if (ret != r.length)
    {
        if (!t)
        {
            disconnect(j.error);
            return;
        }

        // handle_disk_error may disconnect us
        t->on_disk_error(j, this);
        return;
    }

    append_send_buffer(buffer.get(), r.length,
                       boost::bind(&aux::session_impl::free_disk_buffer, boost::ref(m_ses), _1));
    buffer.release();
    base_connection::do_write();

    send_data(left);
}

void peer_connection::receive_data(const peer_request& req)
{
    assert(!m_read_in_progress);
    std::pair<peer_request, peer_request> reqs = split_request(req);
    peer_request r = reqs.first;
    peer_request left = reqs.second;

    if (r.length > 0)
    {
        if (!allocate_disk_receive_buffer(r.length))
        {
            ERR("cannot allocate disk receive buffer " << r.length);
            return;
        }

        boost::asio::async_read(
            *m_socket, boost::asio::buffer(m_disk_recv_buffer.get(), r.length),
            make_read_handler(boost::bind(&peer_connection::on_receive_data,
                                          self_as<peer_connection>(), _1, _2, r, left)));
        m_channel_state[download_channel] = bw_network;
        m_read_in_progress = true;
    }
    else
    {
        m_channel_state[download_channel] = bw_idle;
        do_read();
    }
}

void peer_connection::on_receive_data(
    const error_code& error, std::size_t bytes_transferred, peer_request r, peer_request left)
{
    assert(int(bytes_transferred) == r.length);
    assert(m_channel_state[download_channel] == bw_network);
    assert(m_read_in_progress);
    m_read_in_progress = false;

    boost::shared_ptr<transfer> t = m_transfer.lock();

    if (is_disconnecting()) return;
    if (t->is_seed()) return;

    piece_picker& picker = t->picker();
    piece_manager& fs = t->filesystem();
    piece_block block_finished(r.piece, r.start / BLOCK_SIZE);

    std::vector<pending_block>::iterator b
        = std::find_if(m_download_queue.begin(), m_download_queue.end(), has_block(block_finished));

    if (b == m_download_queue.end())
    {
        ERR("The block we just got was not in the request queue");
        return;
    }

    disk_buffer_holder holder(m_ses.m_disk_thread, release_disk_receive_buffer());
    fs.async_write(r, holder, boost::bind(&peer_connection::on_disk_write_complete,
                                          self_as<peer_connection>(), _1, _2, r, left, t));

    if (left.length > 0)
        receive_data(left);
    else
    {
        bool was_finished = picker.is_piece_finished(r.piece);
        picker.mark_as_writing(block_finished, get_peer());
        m_download_queue.erase(b);

        // did we just finish the piece?
        // this means all blocks are either written
        // to disk or are in the disk write cache
        if (picker.is_piece_finished(r.piece) && !was_finished)
        {
            boost::optional<const md4_hash&> ohash = m_remote_hashset.hash(r.piece);
            assert(ohash);
            t->async_verify_piece(
                r.piece, *ohash, boost::bind(&transfer::piece_finished, t, r.piece, *ohash, _1));
        }

        m_channel_state[download_channel] = bw_idle;
        do_read();
        request_block();
        send_block_requests();
    }
}

void peer_connection::on_disk_write_complete(
    int ret, disk_io_job const& j, peer_request r, peer_request left, boost::shared_ptr<transfer> t)
{
    boost::mutex::scoped_lock l(m_ses.m_mutex);

    assert(r.piece == j.piece);
    assert(r.start == j.offset);

    if (left.length == 0)
    {
        // in case the outstanding bytes just dropped down
        // to allow to receive more data
        //do_read();

        if (t->is_seed()) return;

        piece_block block_finished(r.piece, r.start / BLOCK_SIZE);
        piece_picker& picker = t->picker();
        picker.mark_as_finished(block_finished, get_peer());
    }
}

void peer_connection::on_write(const error_code& error, size_t nSize)
{
    base_connection::on_write(error, nSize);
}

void peer_connection::write_hello()
{
    DBG("hello ==> " << m_remote);
    const session_settings& settings = m_ses.settings();
    boost::uint32_t nVersion = 0x3c;
    client_hello hello;
    hello.m_nHashLength = MD4_HASH_SIZE;
    hello.m_hClient = settings.client_hash;
    hello.m_network_point.m_nIP = m_ses.m_client_id;
    hello.m_network_point.m_nPort = settings.listen_port;
    hello.m_list.add_tag(make_string_tag(settings.client_name, CT_NAME, true));
    hello.m_list.add_tag(make_typed_tag(nVersion, CT_VERSION, true));
    hello.m_server_network_point.m_nIP = address2int(m_ses.server().address());
    hello.m_server_network_point.m_nPort = m_ses.server().port();

    // generate options
    boost::uint32_t nOption2;
    boost::uint32_t nOption1 = 0;
    const boost::uint32_t uSupportsCaptcha       = 1;
    const boost::uint32_t uSupportLargeFiles     = 1;
    const boost::uint32_t uExtMultiPacket        = 1;



    nOption2 = 0xFFFFFFFF; //(uSupportsCaptcha << 11) | (uSupportLargeFiles << 4);

    hello.m_list.add_tag(make_typed_tag(nOption1, CT_EMULE_MISCOPTIONS1, true));
    hello.m_list.add_tag(make_typed_tag(nOption2, CT_EMULE_MISCOPTIONS2, false));

    do_write(hello);
}

void peer_connection::write_hello_answer()
{
    DBG("hello answer ==> " << m_remote);
    //TODO - replace this code
    // prepare hello answer
    client_hello_answer cha;
    cha.m_hClient               = m_ses.settings().client_hash;
    cha.m_network_point.m_nIP  = 0;
    cha.m_network_point.m_nPort= m_ses.settings().listen_port;

    boost::uint32_t nVersion = 0x3c;
    //boost::uint32_t nCapability = CAPABLE_AUXPORT | CAPABLE_NEWTAGS |
    //    CAPABLE_UNICODE | CAPABLE_LARGEFILES;
    //boost::uint32_t nClientVersion  = (3 << 24) | (2 << 17) | (3 << 10) | (1 << 7);
    boost::uint32_t nUdpPort = 0;

    cha.m_list.add_tag(
        make_string_tag(std::string(m_ses.settings().client_name), CT_NAME, true));
    cha.m_list.add_tag(make_typed_tag(nVersion, CT_VERSION, true));
    cha.m_list.add_tag(make_typed_tag(nUdpPort, CT_EMULE_UDPPORTS, true));
    cha.m_server_network_point.m_nPort = m_ses.settings().server_port;
    cha.m_server_network_point.m_nIP   = m_ses.settings().server_ip;

    do_write(cha);
}

void peer_connection::write_file_request(const md4_hash& file_hash)
{
    DBG("file request " << file_hash << " ==> " << m_remote);
    client_file_request fr;
    fr.m_hFile = file_hash;
    do_write(fr);
}

void peer_connection::write_file_answer(
    const md4_hash& file_hash, const std::string& filename)
{
    DBG("file answer " << file_hash << ", " << filename << " ==> " << m_remote);
    client_file_answer fa;
    fa.m_hFile = file_hash;
    fa.m_filename.m_collection = filename;
    do_write(fa);
}

void peer_connection::write_no_file(const md4_hash& file_hash)
{
    DBG("no file " << file_hash << " ==> " << m_remote);
    client_no_file nf;
    nf.m_hFile = file_hash;
    do_write(nf);
}

void peer_connection::write_filestatus_request(const md4_hash& file_hash)
{
    DBG("request file status " << file_hash << " ==> " << m_remote);
    client_filestatus_request fr;
    fr.m_hFile = file_hash;
    do_write(fr);
}

void peer_connection::write_file_status(
    const md4_hash& file_hash, const bitfield& status)
{
    DBG("file status " << file_hash
        << ", [" << bitfield2string(status) << "] ==> " << m_remote);
    client_file_status fs;
    fs.m_hFile = file_hash;
    fs.m_status = status;
    do_write(fs);
}

void peer_connection::write_hashset_request(const md4_hash& file_hash)
{
    DBG("request hashset for " << file_hash << " ==> " << m_remote);
    client_hashset_request hr;
    hr.m_hFile = file_hash;
    do_write(hr);
}

void peer_connection::write_hashset_answer(
    const md4_hash& file_hash, const std::vector<md4_hash>& hash_set)
{
    DBG("hashset ==> " << m_remote);
    client_hashset_answer ha;
    ha.m_hFile = file_hash;
    ha.m_vhParts.m_collection = hash_set;
    do_write(ha);
}

void peer_connection::write_start_upload(const md4_hash& file_hash)
{
    DBG("start download " << file_hash << " ==> " << m_remote);
    client_start_upload su;
    su.m_hFile = file_hash;
    do_write(su);
}

void peer_connection::write_queue_ranking(boost::uint16_t rank)
{
    DBG("queue ranking is " << rank << " ==> " << m_remote);
    client_queue_ranking qr;
    qr.m_nRank = rank;
    do_write(qr);
}

void peer_connection::write_accept_upload()
{
    DBG("accept upload ==> " << m_remote);
    client_accept_upload au;
    do_write(au);
}

void peer_connection::write_cancel_transfer()
{
    DBG("cancel ==> " << m_remote);
    client_cancel_transfer ct;
    do_write(ct);
}

void peer_connection::write_request_parts(client_request_parts_64 rp)
{
    DBG("request parts " << rp.m_hFile << ": "
        << "[" << rp.m_begin_offset[0] << ", " << rp.m_end_offset[0] << "]"
        << "[" << rp.m_begin_offset[1] << ", " << rp.m_end_offset[1] << "]"
        << "[" << rp.m_begin_offset[2] << ", " << rp.m_end_offset[2] << "]"
        << " ==> " << m_remote);
    do_write(rp);
}

void peer_connection::write_part(const peer_request& r)
{
    boost::shared_ptr<transfer> t = m_transfer.lock();
    client_sending_part_64 sp;
    std::pair<size_t, size_t> range = block_range(r.piece, r.start / BLOCK_SIZE, t->filesize());
    sp.m_hFile = t->hash();
    sp.m_begin_offset = range.first;
    sp.m_end_offset = range.second;
    do_write(sp);

    DBG("part " << sp.m_hFile << " [" << sp.m_begin_offset << ", " << sp.m_end_offset << "]"
        << " ==> " << m_remote);
}

void peer_connection::on_hello(const error_code& error)
{
    DBG("hello <== " << m_remote);
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
        client_hello_answer cha;
        if (decode_packet(cha))
        {
            // extract user info from packet
            for (size_t n = 0; n < cha.m_list.count(); ++n)
            {
                const boost::shared_ptr<base_tag> p = cha.m_list[n];

                switch(p->getNameId())
                {
                    case CT_NAME:
                        m_strName    = p->asString();
                        break;

                    case CT_VERSION:
                        m_nVersion  = p->asInt();
                        break;

                    case ET_MOD_VERSION:
                        if (is_string_tag(p))
                        {
                            m_strModVersion = p->asString();
                        }
                        else if (is_int_tag(p))
                        {
                            m_nModVersion = p->asInt();
                        }
                        break;

                    case CT_PORT:
                        m_nPort = p->asInt();
                        break;

                    case CT_EMULE_UDPPORTS:
                        m_nUDPPort = p->asInt() & 0xFFFF;
                        //dwEmuleTags |= 1;
                        break;

                    case CT_EMULE_BUDDYIP:
                        // 32 BUDDY IP
                        m_nBuddyIP = p->asInt();
                        break;

                    case CT_EMULE_BUDDYUDP:
                        m_nBuddyPort = p->asInt();
                        break;

                    case CT_EMULE_MISCOPTIONS1: {
                        //  3 AICH Version (0 = not supported)
                        //  1 Unicode
                        //  4 UDP version
                        //  4 Data compression version
                        //  4 Secure Ident
                        //  4 Source Exchange
                        //  4 Ext. Requests
                        //  4 Comments
                        //   1 PeerCache supported
                        //   1 No 'View Shared Files' supported
                        //   1 MultiPacket
                        //  1 Preview
                        /*
                        uint32 flags = temptag.GetInt();
                        m_fSupportsAICH         = (flags >> (4*7+1)) & 0x07;
                        m_bUnicodeSupport       = (flags >> 4*7) & 0x01;
                        m_byUDPVer              = (flags >> 4*6) & 0x0f;
                        m_byDataCompVer         = (flags >> 4*5) & 0x0f;
                        m_bySupportSecIdent     = (flags >> 4*4) & 0x0f;
                        m_bySourceExchange1Ver  = (flags >> 4*3) & 0x0f;
                        m_byExtendedRequestsVer = (flags >> 4*2) & 0x0f;
                        m_byAcceptCommentVer    = (flags >> 4*1) & 0x0f;
                        m_fNoViewSharedFiles    = (flags >> 1*2) & 0x01;
                        m_bMultiPacket          = (flags >> 1*1) & 0x01;
                        m_fSupportsPreview      = (flags >> 1*0) & 0x01;
                        dwEmuleTags |= 2;

                        SecIdentSupRec +=  1;
                        */
                        break;
                    }

                    case CT_EMULE_MISCOPTIONS2:
                        //  19 Reserved
                        //   1 Direct UDP Callback supported and available
                        //   1 Supports ChatCaptchas
                        //   1 Supports SourceExachnge2 Packets, ignores SX1 Packet Version
                        //   1 Requires CryptLayer
                        //   1 Requests CryptLayer
                        //   1 Supports CryptLayer
                        //   1 Reserved (ModBit)
                        //   1 Ext Multipacket (Hash+Size instead of Hash)
                        //   1 Large Files (includes support for 64bit tags)
                        //   4 Kad Version - will go up to version 15 only (may need to add another field at some point in the future)
                        /*
                        m_fDirectUDPCallback    = (temptag.GetInt() >> 12) & 0x01;
                        m_fSupportsCaptcha      = (temptag.GetInt() >> 11) & 0x01;
                        m_fSupportsSourceEx2    = (temptag.GetInt() >> 10) & 0x01;
                        m_fRequiresCryptLayer   = (temptag.GetInt() >>  9) & 0x01;
                        m_fRequestsCryptLayer   = (temptag.GetInt() >>  8) & 0x01;
                        m_fSupportsCryptLayer   = (temptag.GetInt() >>  7) & 0x01;
                        // reserved 1
                        m_fExtMultiPacket   = (temptag.GetInt() >>  5) & 0x01;
                        m_fSupportsLargeFiles   = (temptag.GetInt() >>  4) & 0x01;
                        m_byKadVersion      = (temptag.GetInt() >>  0) & 0x0f;
                        dwEmuleTags |= 8;

                        m_fRequestsCryptLayer &= m_fSupportsCryptLayer;
                        m_fRequiresCryptLayer &= m_fRequestsCryptLayer;
       */
                        break;

                    // Special tag for Compat. Clients Misc options.
                    case CT_EMULECOMPAT_OPTIONS:
                        //  1 Operative System Info
                        //  1 Value-based-type int tags (experimental!)
                        //m_fValueBasedTypeTags   = (temptag.GetInt() >> 1*1) & 0x01;
                        //m_fOsInfoSupport        = (temptag.GetInt() >> 1*0) & 0x01;
                        break;

                    case CT_EMULE_VERSION:
                        //  8 Compatible Client ID
                        //  7 Mjr Version (Doesn't really matter..)
                        //  7 Min Version (Only need 0-99)
                        //  3 Upd Version (Only need 0-5)
                        //  7 Bld Version (Only need 0-99)
                        //m_byCompatibleClient = (temptag.GetInt() >> 24);
                        //m_nClientVersion = temptag.GetInt() & 0x00ffffff;
                        //m_byEmuleVersion = 0x99;
                        //m_fSharedDirectories = 1;
                        //dwEmuleTags |= 4;
                        break;
                    default:
                        break;
                }
            }// for

            m_ses.m_alerts.post_alert_should(peer_connected_alert(address2int(m_remote.address()), cha, m_active));
        }

        // peer handshake completed
        boost::shared_ptr<transfer> t = m_transfer.lock();

        if (t)
        {
            write_file_request(t->hash());
            DBG("write file request");
        }
        else
        {
            DBG("fill send buffer");
            fill_send_buffer();
        }
    }
    else
    {
        ERR("hello answer received error " << error.message());
    }
}

void peer_connection::on_file_request(const error_code& error)
{
    if (!error)
    {
        client_file_request fr;
        decode_packet(fr);
        DBG("file request " << fr.m_hFile << " <== " << m_remote);
        if (attach_to_transfer(fr.m_hFile))
        {
            boost::shared_ptr<transfer> t = m_transfer.lock();
            write_file_status(fr.m_hFile, t->hashset().pieces());
        }
        else
        {
            write_no_file(fr.m_hFile);
            disconnect(errors::file_unavaliable, 2);
        }
    }
    else
    {
        ERR("file request error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_file_answer(const error_code& error)
{
    if (!error)
    {
        client_file_answer fa;
        decode_packet(fa);
        DBG("file answer " << fa.m_hFile << ", " << fa.m_filename.m_collection
            << " <== " << m_remote);
        write_filestatus_request(fa.m_hFile);
    }
    else
    {
        ERR("file answer error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_file_description(const error_code& error)
{
    if (!error)
    {
        client_file_description fd;
        decode_packet(fd);
        DBG("file description " << fd.m_nRating << ", " << fd.m_sComment.m_collection << " <== " << m_remote);
    }
    else
    {
        ERR("file description error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_no_file(const error_code& error)
{
    if (!error)
    {
        client_no_file nf;
        decode_packet(nf);
        DBG("no file " << nf.m_hFile << m_remote);
        disconnect(errors::file_unavaliable, 2);
    }
    else
    {
        ERR("no file error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_filestatus_request(const error_code& error)
{
    if (!error)
    {
        client_filestatus_request fr;
        decode_packet(fr);
        DBG("file status request " << fr.m_hFile << " <== " << m_remote);

        boost::shared_ptr<transfer> t = m_transfer.lock();
        if (t->hash() == fr.m_hFile)
        {
            write_file_status(t->hash(), t->hashset().pieces());
        }
        else
        {
            write_no_file(fr.m_hFile);
            disconnect(errors::file_unavaliable, 2);
        }
    }
    else
    {
        ERR("file status request error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_file_status(const error_code& error)
{
    if (!error)
    {
        boost::shared_ptr<transfer> t = m_transfer.lock();
        client_file_status fs;

        decode_packet(fs);
        if (fs.m_status.size() == 0)
            fs.m_status.resize(t->num_pieces(), 1);

        DBG("file status answer "<< fs.m_hFile
            << ", [" << bitfield2string(fs.m_status) << "] <== " << m_remote);

        if (t->hash() == fs.m_hFile)
        {
            m_remote_hashset.pieces(fs.m_status);
            t->picker().inc_refcount(fs.m_status);
            write_hashset_request(fs.m_hFile);
        }
        else
        {
            write_no_file(fs.m_hFile);
            disconnect(errors::file_unavaliable, 2);
        }
    }
    else
    {
        ERR("file status answer error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_hashset_request(const error_code& error)
{
    if (!error)
    {
        client_hashset_request hr;
        decode_packet(hr);
        DBG("hash set request " << hr.m_hFile << " <== " << m_remote);

        boost::shared_ptr<transfer> t = m_transfer.lock();
        if (t->hash() == hr.m_hFile)
        {
            write_hashset_answer(t->hash(), t->hashset().hashes());
        }
        else
        {
            write_no_file(hr.m_hFile);
            disconnect(errors::file_unavaliable, 2);
        }
    }
    else
    {
        ERR("hashset request error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_hashset_answer(const error_code& error)
{
    if (!error)
    {
        client_hashset_answer ha;
        decode_packet(ha);
        DBG("hash set answer " << ha.m_hFile << " <== " << m_remote);

        boost::shared_ptr<transfer> t = m_transfer.lock();
        if (t->hash() == ha.m_hFile)
        {
            m_remote_hashset.hashes(ha.m_vhParts.m_collection);
            write_start_upload(t->hash());
        }
        else
        {
            write_no_file(ha.m_hFile);
            disconnect(errors::file_unavaliable, 2);
        }
    }
    else
    {
        ERR("hashset answer error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_start_upload(const error_code& error)
{
    if (!error)
    {
        client_start_upload su;
        decode_packet(su);
        DBG("start upload " << su.m_hFile << " <== " << m_remote);

        boost::shared_ptr<transfer> t = m_transfer.lock();
        if (t->hash() == su.m_hFile)
        {
            write_accept_upload();
        }
        else
        {
            write_no_file(su.m_hFile);
            disconnect(errors::file_unavaliable, 2);
        }
    }
    else
    {
        ERR("start upload error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_queue_ranking(const error_code& error)
{
    if (!error)
    {
        client_queue_ranking qr;
        decode_packet(qr);
        DBG("queue ranking " << qr.m_nRank << " <== " << m_remote);
        // TODO: handle it
    }
    else
    {
        ERR("queue ranking error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_accept_upload(const error_code& error)
{
    if (!error)
    {
        DBG("accept upload <== " << m_remote);
        request_block();
        send_block_requests();
    }
    else
    {
        ERR("accept upload error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_out_parts(const error_code& error)
{
    if (!error)
    {
        client_out_parts op;
        decode_packet(op);
        DBG("out of parts <== " << m_remote);
    }
    else
    {
        ERR("out of parts error " << error.message() << " <== " << m_remote);
    }

}

void peer_connection::on_cancel_transfer(const error_code& error)
{
    if (!error)
    {
        DBG("cancel transfer <== " << m_remote);
        // TODO: handle it.
        disconnect(errors::transfer_aborted);
    }
    else
    {
        ERR("transfer cancel error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_request_parts(const error_code& error)
{
    if (!error)
    {
        client_request_parts_64 rp;
        decode_packet(rp);
        DBG("request parts " << rp.m_hFile << ": "
            << "[" << rp.m_begin_offset[0] << ", " << rp.m_end_offset[0] << "]"
            << "[" << rp.m_begin_offset[1] << ", " << rp.m_end_offset[1] << "]"
            << "[" << rp.m_begin_offset[2] << ", " << rp.m_end_offset[2] << "]"
            << " <== " << m_remote);
        for (size_t i = 0; i < 3; ++i)
        {
            if (rp.m_begin_offset[i] < rp.m_end_offset[i])
            {
                m_requests.push_back(mk_peer_request(rp.m_begin_offset[i], rp.m_end_offset[i]));
            }
        }
        fill_send_buffer();
    }
    else
    {
        ERR("request parts error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_end_download(const error_code& error)
{
    if (!error)
    {
        client_end_download ed;
        decode_packet(ed);
        DBG("end download " << ed.m_hFile << " <== " << m_remote);
    }
    else
    {
        ERR("end download error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_shared_files_answer(const error_code& error)
{
    if (!error)
    {
        DECODE_PACKET(client_shared_files_answer);
        m_ses.m_alerts.post_alert_should(shared_files_alert(address2int(m_remote.address()), packet.m_files, false));
    }
    else
    {
        ERR("peer_connection::on_shared_files_answer(" << error.message() << ")");
    }
}

void peer_connection::on_client_message(const error_code& error)
{
    if (!error)
    {

        DECODE_PACKET(client_message)
        m_ses.m_alerts.post_alert_should(peer_message_alert(address2int(m_remote.address()), packet.m_strMessage));
    }
    else
    {
        ERR("on client message error: " << error.message());
    }
}

void peer_connection::on_client_captcha_request(const error_code& error)
{
    if (!error)
    {
        DECODE_PACKET(client_captcha_request)
        m_ses.m_alerts.post_alert_should(peer_captcha_request_alert(address2int(m_remote.address()), packet.m_captcha));

    }
    else
    {
        ERR("on client captcha request error: " << error.message());
    }
}

void peer_connection::on_client_captcha_result(const error_code& error)
{
    if (!error)
    {
        DECODE_PACKET(client_captcha_result)
        m_ses.m_alerts.post_alert_should(peer_captcha_result_alert(address2int(m_remote.address()), packet.m_captcha_result));
    }
    else
    {
        ERR("on client captcha result error: " << error.message());
    }
}

