#include <boost/foreach.hpp>
#include <boost/format.hpp>

#include <libtorrent/piece_picker.hpp>
#include <libtorrent/storage.hpp>

#include "libed2k/peer_connection.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/session.hpp"
#include "libed2k/transfer.hpp"
#include "libed2k/file.hpp"
#include "libed2k/util.hpp"
#include "libed2k/alert_types.hpp"
#include "libed2k/server_connection.hpp"

namespace libed2k
{
    EClientSoftware uagent2csoft(const md4_hash& ua_hash)
    {
        EClientSoftware cs = SO_UNKNOWN;

        if ( ua_hash[5] == 13  && ua_hash[14] == 110 )
        {
            cs =  SO_OLDEMULE;
        }
        else if ( ua_hash[5] == 14  && ua_hash[14] == 111 )
        {
            cs = SO_EMULE;
        }
        else if ( ua_hash[5] == 'M' && ua_hash[14] == 'L' )
        {
            cs = SO_MLDONKEY;
        }
        else if ( ua_hash[5] == 'L' && ua_hash[14] == 'K')
        {
            cs = SO_LIBED2K;
        }
        else if ( ua_hash[5] == 'Q' && ua_hash[14] == 'M')
        {
            cs = SO_QMULE;
        }

        return cs;
    }
}


using namespace libed2k;
namespace ip = boost::asio::ip;

peer_request mk_peer_request(fsize_t begin, fsize_t end)
{
    peer_request r;
    r.piece = begin / PIECE_SIZE;
    r.start = begin % PIECE_SIZE;
    r.length = end - begin;
    return r;
}

std::pair<fsize_t, fsize_t> mk_range(const peer_request& r)
{
    fsize_t begin = r.piece * PIECE_SIZE + r.start;
    fsize_t end = begin + r.length;
    assert(begin < end);
    return std::make_pair(begin, end);
}

inline piece_block mk_block(const peer_request& r)
{
    return piece_block(r.piece, r.start / BLOCK_SIZE);
}

inline size_t offset_in_block(const peer_request& r)
{
    return r.start % BLOCK_SIZE;
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

size_t block_size(const piece_block& b, fsize_t s)
{
    std::pair<fsize_t, fsize_t> r = block_range(b.piece_index, b.block_index, s);
    return size_t(r.second - r.first);
}

peer_connection::peer_connection(aux::session_impl& ses,
                                 boost::weak_ptr<transfer> transfer,
                                 boost::shared_ptr<tcp::socket> s,
                                 const ip::tcp::endpoint& remote, peer* peerinfo):
    base_connection(ses, s, remote),
    m_work(ses.m_io_service),
    m_disk_recv_buffer(ses.m_disk_thread, 0),
    m_transfer(transfer),
    m_peer(peerinfo),
    m_connecting(true),
    m_active(true),
    m_disk_recv_buffer_size(0),
    m_handshake_complete(false)
{
    reset();
}

peer_connection::peer_connection(aux::session_impl& ses,
                                 boost::shared_ptr<tcp::socket> s,
                                 const ip::tcp::endpoint& remote,
                                 peer* peerinfo):
    base_connection(ses, s, remote),
    m_work(ses.m_io_service),
    m_disk_recv_buffer(ses.m_disk_thread, 0),
    m_peer(peerinfo),
    m_connecting(false),
    m_active(false),
    m_disk_recv_buffer_size(0),
    m_handshake_complete(false)
{
    reset();

    // connection is already established
    do_read();
}

void peer_connection::reset()
{
    m_last_receive = time_now();
    m_last_sent = time_now();
    m_timeout = time::seconds(m_ses.settings().peer_timeout);

    m_disconnecting = false;
    m_connection_ticket = -1;
    m_speed = slow;
    m_desired_queue_size = 32;
    m_max_busy_blocks = 1;

    m_channel_state[upload_channel] = bw_idle;
    m_channel_state[download_channel] = bw_idle;

    add_handler(std::make_pair(OP_HELLO, OP_EDONKEYPROT), boost::bind(&peer_connection::on_hello, this, _1));
    add_handler(get_proto_pair<client_hello_answer>(), boost::bind(&peer_connection::on_hello_answer, this, _1));
    add_handler(get_proto_pair<client_ext_hello>(), boost::bind(&peer_connection::on_ext_hello, this, _1));
    add_handler(get_proto_pair<client_ext_hello_answer>(), boost::bind(&peer_connection::on_ext_hello_answer, this, _1));
    add_handler(get_proto_pair<client_file_request>(), boost::bind(&peer_connection::on_file_request, this, _1));
    add_handler(get_proto_pair<client_file_answer>(), boost::bind(&peer_connection::on_file_answer, this, _1));
    add_handler(/*OP_FILEDESC*/get_proto_pair<client_file_description>(), boost::bind(&peer_connection::on_file_description, this, _1));
    add_handler(/*OP_SETREQFILEID*/get_proto_pair<client_filestatus_request>(), boost::bind(&peer_connection::on_filestatus_request, this, _1));
    add_handler(/*OP_FILEREQANSNOFIL*/get_proto_pair<client_no_file>(), boost::bind(&peer_connection::on_no_file, this, _1));
    add_handler(/*OP_FILESTATUS*/get_proto_pair<client_file_status>(), boost::bind(&peer_connection::on_file_status, this, _1));
    add_handler(/*OP_HASHSETREQUEST*/get_proto_pair<client_hashset_request>(), boost::bind(&peer_connection::on_hashset_request, this, _1));
    add_handler(/*OP_HASHSETANSWER*/get_proto_pair<client_hashset_answer>(), boost::bind(&peer_connection::on_hashset_answer, this, _1));
    add_handler(/*OP_STARTUPLOADREQ*/get_proto_pair<client_start_upload>(), boost::bind(&peer_connection::on_start_upload, this, _1));
    add_handler(/*OP_QUEUERANKING*/get_proto_pair<client_queue_ranking>(), boost::bind(&peer_connection::on_queue_ranking, this, _1));
    add_handler(std::make_pair(OP_ACCEPTUPLOADREQ, OP_EDONKEYPROT), boost::bind(&peer_connection::on_accept_upload, this, _1));
    add_handler(/*OP_OUTOFPARTREQS*/get_proto_pair<client_out_parts>(), boost::bind(&peer_connection::on_out_parts, this, _1));
    add_handler(std::make_pair(OP_CANCELTRANSFER, OP_EDONKEYPROT), boost::bind(&peer_connection::on_cancel_transfer, this, _1));
    add_handler(/*OP_REQUESTPARTS*/get_proto_pair<client_request_parts_32>(),
                boost::bind(&peer_connection::on_request_parts<client_request_parts_32>, this, _1));
    add_handler(/*OP_REQUESTPARTS_I64*/get_proto_pair<client_request_parts_64>(),
                boost::bind(&peer_connection::on_request_parts<client_request_parts_64>, this, _1));
    add_handler(/*OP_SENDINGPART*/get_proto_pair<client_sending_part_32>(),
                boost::bind(&peer_connection::on_sending_part<client_sending_part_32>, this, _1));
    add_handler(/*OP_SENDINGPART_I64*/get_proto_pair<client_sending_part_64>(),
                boost::bind(&peer_connection::on_sending_part<client_sending_part_64>, this, _1));
    add_handler(/*OP_END_OF_DOWNLOAD*/get_proto_pair<client_end_download>(), boost::bind(&peer_connection::on_end_download, this, _1));

    // shared files request and answer
    add_handler(/*OP_ASKSHAREDFILES*/get_proto_pair<client_shared_files_request>(), boost::bind(&peer_connection::on_shared_files_request, this, _1));
    add_handler(/*OP_ASKSHAREDDENIEDANS*/get_proto_pair<client_shared_files_denied>(), boost::bind(&peer_connection::on_shared_files_denied, this, _1));
    add_handler(/*OP_ASKSHAREDFILESANSWER*/get_proto_pair<client_shared_files_answer>(), boost::bind(&peer_connection::on_shared_files_answer, this, _1));

    // shared directories
    add_handler(get_proto_pair<client_shared_directories_request>(), boost::bind(&peer_connection::on_shared_directories_request, this, _1));
    add_handler(get_proto_pair<client_shared_directories_answer>(), boost::bind(&peer_connection::on_shared_directories_answer, this, _1));

    // shared files in directory
    add_handler(get_proto_pair<client_shared_directory_files_answer>(), boost::bind(&peer_connection::on_shared_directory_files_answer, this, _1));

    //ismod collections
    add_handler(get_proto_pair<client_directory_content_request>(), boost::bind(&peer_connection::on_ismod_files_request, this, _1));
    add_handler(get_proto_pair<client_directory_content_result>(), boost::bind(&peer_connection::on_ismod_directory_files, this, _1));
    // clients talking
    add_handler(/*OP_MESSAGE*/get_proto_pair<client_message>(), boost::bind(&peer_connection::on_client_message, this, _1));
    add_handler(/*OP_CHATCAPTCHAREQ*/get_proto_pair<client_captcha_request>(), boost::bind(&peer_connection::on_client_captcha_request, this, _1));
    add_handler(/*OP_CHATCAPTCHARES*/get_proto_pair<client_captcha_result>(), boost::bind(&peer_connection::on_client_captcha_result, this, _1));

}

peer_connection::~peer_connection()
{
    m_disk_recv_buffer_size = 0;
}

void peer_connection::init()
{
}

void peer_connection::get_peer_info(peer_info& p) const
{
    p.down_speed = m_statistics.download_rate();
    p.up_speed = m_statistics.upload_rate();
    p.payload_down_speed = m_statistics.download_payload_rate();
    p.payload_up_speed = m_statistics.upload_payload_rate();
    p.num_pieces = m_remote_pieces.count();

    p.total_download = m_statistics.total_payload_download();
    p.total_upload = m_statistics.total_payload_upload();
    p.ip = m_remote;
    p.connection_type = STANDARD_EDONKEY;
    p.client = !m_options.m_strName.empty() && m_options.m_strName[0] == '[' ?
        m_options.m_strName :
        (boost::format("[%1%] %2%") % m_options.m_strModVersion % m_options.m_strName).str();

    if (boost::optional<piece_block_progress> ret = downloading_piece_progress())
    {
        p.downloading_piece_index = ret->piece_index;
        p.downloading_block_index = ret->block_index;
        p.downloading_progress = ret->bytes_downloaded;
        p.downloading_total = ret->full_block_bytes;
    }
    else
    {
        p.downloading_piece_index = -1;
        p.downloading_block_index = -1;
        p.downloading_progress = 0;
        p.downloading_total = 0;
    }

    p.pieces = m_remote_pieces;

    // this will set the flags so that we can update them later
    p.flags = 0;
    p.flags |= is_seed() ? peer_info::seed : 0;

    p.source = 0;
    p.failcount = 0;
    p.num_hashfails = 0;
    p.inet_as = 0xffff;

    p.send_buffer_size = m_send_buffer.capacity();
    p.used_send_buffer = m_send_buffer.size();
    p.write_state = m_channel_state[upload_channel];
    p.read_state = m_channel_state[download_channel];

    // pieces may be empty if we don't have metadata yet
    if (p.pieces.size() == 0)
    {
        p.progress = 0.f;
        p.progress_ppm = 0;
    }
    else
    {
        p.progress = (float)p.pieces.count() / (float)p.pieces.size();
        p.progress_ppm = boost::uint64_t(p.pieces.count()) * 1000000 / p.pieces.size();
    }
}

peer_connection::peer_speed_t peer_connection::peer_speed()
{
    boost::shared_ptr<transfer> t = m_transfer.lock();
    assert(t);

    int download_rate = int(statistics().download_payload_rate());
    int transfer_download_rate = int(t->statistics().download_payload_rate());

    if (download_rate > 512 && download_rate > transfer_download_rate / 16)
        m_speed = fast;
    else if (download_rate > 4096 && download_rate > transfer_download_rate / 64)
        m_speed = medium;
    else if (download_rate < transfer_download_rate / 15 && m_speed == fast)
        m_speed = medium;
    else
        m_speed = slow;

    return m_speed;
}

double peer_connection::peer_rate()
{
    boost::shared_ptr<transfer> t = m_transfer.lock();

    fsize_t downloaded = statistics().total_download();
    fsize_t transfer_downloaded = t->statistics().total_download();
    return (transfer_downloaded == 0 ? 0 : double(downloaded) / transfer_downloaded) * t->num_peers();
}

void peer_connection::second_tick(int tick_interval_ms)
{
    ptime now = time_now();
    boost::intrusive_ptr<peer_connection> me(self_as<peer_connection>());

    // if the peer hasn't said a thing for a certain
    // time, it is considered to have timed out
    time_duration d = now - m_last_receive;
    // if we can't read, it means we're blocked on the rate-limiter
    // or the disk, not the peer itself. In this case, don't blame
    // the peer and disconnect it
    bool may_timeout = can_read();
    if (may_timeout && d > m_timeout && !m_connecting)
    {
        disconnect(errors::timed_out_inactivity);
        return;
    }

    abort_expired_requests();

    if (!is_closed())
    {
        fill_send_buffer();
    }

    m_statistics.second_tick(tick_interval_ms);
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

    if (t == m_transfer.lock())
    {
        // this connection is already attached to the transfer
        return true;
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

    if (m_disconnecting) return;
    if (!t || t->is_seed() || !can_request()) return;

    int num_requests = m_desired_queue_size
        - (int)m_download_queue.size()
        - (int)m_request_queue.size();

    // if our request queue is already full, we
    // don't have to make any new requests yet
    if (num_requests <= 0) return;

    piece_picker& p = t->picker();
    std::vector<piece_block> interesting_pieces;
    interesting_pieces.reserve(100);

    int prefer_whole_pieces = 0;

    piece_picker::piece_state_t state;
    peer_speed_t speed = peer_speed();
    if (speed == fast) state = piece_picker::fast;
    else if (speed == medium) state = piece_picker::medium;
    else state = piece_picker::slow;

    p.pick_pieces(m_remote_pieces, interesting_pieces,
                  num_requests, prefer_whole_pieces, m_peer,
                  state, picker_options(), std::vector<int>());

    // if the number of pieces we have + the number of pieces
    // we're requesting from is less than the number of pieces
    // in the transfer, there are still some unrequested pieces
    // also, if we already have at least one outstanding
    // request, we shouldn't pick any busy pieces either
    bool dont_pick_busy_blocks =
        (p.num_have() + p.get_download_queue().size() < t->num_pieces()) ||
        m_download_queue.size() + m_request_queue.size() > 0;

    // this is filled with an interesting piece
    // that some other peer is currently downloading
    std::vector<piece_block> busy_blocks;

    for (std::vector<piece_block>::iterator i = interesting_pieces.begin();
         i != interesting_pieces.end(); ++i)
    {
        int num_block_requests = p.num_peers(*i);
        if (num_block_requests > 0)
        {
            // have we picked enough pieces?
            if (num_requests <= 0) break;

            // this block is busy. This means all the following blocks
            // in the interesting_pieces list are busy as well, we might
            // as well just exit the loop
            if (dont_pick_busy_blocks || busy_blocks.size() >= m_max_busy_blocks) break;

            assert(p.num_peers(*i) > 0);
            busy_blocks.push_back(*i);
            continue;
        }

        // don't request pieces we already have in our request queue
        // This happens when pieces time out or the peer sends us
        // pieces we didn't request. Those aren't marked in the
        // piece picker, but we still keep track of them in the
        // download queue
        if (requesting(*i)) continue;

        // ok, we found a piece that's not being downloaded
        // by somebody else. request it from this peer
        // and return
        if (!add_request(*i, 0)) continue;
        num_requests--;
    }

    // if we don't have any potential busy blocks to request
    // or if we already have outstanding requests, don't
    // pick a busy piece
    if (!busy_blocks.empty() && m_download_queue.size() + m_request_queue.size() == 0)
    {
        BOOST_FOREACH(piece_block& bb, busy_blocks) {
            assert(p.is_requested(bb));
            assert(!p.is_downloaded(bb));
            assert(!p.is_finished(bb));
            assert(p.num_peers(bb) > 0);

            add_request(bb, peer_connection::req_busy);
        }
    }
}

bool peer_connection::add_request(const piece_block& block, int flags)
{
    boost::shared_ptr<transfer> t = m_transfer.lock();

    if (!t || m_disconnecting) return false;

    piece_picker::piece_state_t state;
    peer_speed_t speed = peer_speed();
    if (speed == fast) state = piece_picker::fast;
    else if (speed == medium) state = piece_picker::medium;
    else state = piece_picker::slow;

    if ((flags & req_busy) && num_requesting_busy_blocks() >= m_max_busy_blocks)
    {
        // this block is busy (i.e. it has been requested
        // from another peer already). Only allow m_max_busy_blocks busy
        // requests in the pipeline at the time

        return false;
    }

    if (!t->picker().mark_as_downloading(block, get_peer(), state))
        return false;

    pending_block pb(block, t->filesize());
    m_request_queue.push_back(pb);
    return true;
}

void peer_connection::send_block_requests()
{
    if (m_channel_state[upload_channel] != bw_idle) return;

    boost::shared_ptr<transfer> t = m_transfer.lock();
    if (!t || m_disconnecting) return;

    // send in 3 requests at a time
    if (m_download_queue.size() + 2 >= m_desired_queue_size) return;

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

void peer_connection::abort_block_requests()
{
    boost::shared_ptr<transfer> t = m_transfer.lock();

    if (t && t->has_picker())
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
}

void peer_connection::abort_expired_requests()
{
#define ABORT_EXPIRED(reqs)                                             \
    for(std::vector<pending_block>::iterator pi = reqs.begin(); pi != reqs.end();) \
    {                                                                   \
        if (now - pi->create_time > time::seconds(settings.block_request_timeout)) \
        {                                                               \
            piece_block& b = pi->block;                                 \
            DBG("abort expired block request: "                         \
                "{piece: " << b.piece_index << ", block: " << b.block_index << \
                ", remote: " << m_remote << "}");                       \
            picker.abort_download(b);                                   \
            pi = reqs.erase(pi);                                        \
        }                                                               \
        else                                                            \
            ++pi;                                                       \
    }

    boost::shared_ptr<transfer> t = m_transfer.lock();
    const session_settings& settings = m_ses.settings();

    if (t && t->has_picker())
    {
        piece_picker& picker = t->picker();
        ptime now = time_now();

        ABORT_EXPIRED(m_download_queue);
        ABORT_EXPIRED(m_request_queue);
    }

#undef ABORT_EXPIRED
}

bool peer_connection::requesting(const piece_block& b)
{
    return
        std::find_if(m_download_queue.begin(), m_download_queue.end(),
                     has_block(b)) != m_download_queue.end() ||
        std::find_if(m_request_queue.begin(), m_request_queue.end(),
                     has_block(b)) != m_request_queue.end();
}

size_t peer_connection::num_requesting_busy_blocks()
{
    size_t res = 0;

    for (std::vector<pending_block>::const_iterator i = m_download_queue.begin(),
             end(m_download_queue.end()); i != end; ++i)
    {
        if (i->busy) ++res;
    }

    for (std::vector<pending_block>::const_iterator i = m_request_queue.begin(),
             end(m_request_queue.end()); i != end; ++i)
    {
        if (i->busy) ++res;
    }

    return res;
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
    disconnect(errors::timed_out, 1);
}

void peer_connection::disconnect(error_code const& ec, int error)
{
    // TODO: implement

    //if (error > 0) m_failed = true;
    if (m_disconnecting) return;

    boost::intrusive_ptr<peer_connection> me(this);

    if (m_connecting && m_connection_ticket >= 0)
    {
        m_ses.m_half_open.done(m_connection_ticket);
        m_connection_ticket = -1;
    }

    boost::shared_ptr<transfer> t = m_transfer.lock();

    if (t)
    {
        // make sure we keep all the stats!
        t->add_stats(m_statistics);
        abort_block_requests();
        //m_queued_time_critical = 0;

        t->remove_peer(this);
        m_transfer.reset();
    }

    m_disconnecting = true;
    base_connection::close(ec); // close transport
    m_ses.close_connection(this, ec);
    m_ses.m_alerts.post_alert_should(
        peer_disconnected_alert(get_network_point(), get_connection_hash(), ec));
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
    m_last_receive = time_now();

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

net_identifier peer_connection::get_network_point() const
{
    return net_identifier(address2int(m_remote.address()), (m_active)?m_remote.port():m_options.m_nPort);
}

void peer_connection::on_error(const error_code& error)
{
    disconnect(error);
}

void peer_connection::close(const error_code& ec)
{
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

bool peer_connection::can_request() const
{
    boost::shared_ptr<transfer> t = m_transfer.lock();
    return t && t->num_pieces() == m_remote_pieces.size();
}

bool peer_connection::is_seed() const
{
    const bitfield& pieces = m_remote_pieces;
    int pieces_count  = pieces.count();
    return pieces_count == (int)pieces.size() && pieces_count > 0;
}

bool peer_connection::has_network_point(const net_identifier& np) const
{
    bool bRet = false;

    if (m_active)
    {
        bRet = (address2int(m_remote.address()) == np.m_nIP && m_remote.port() == np.m_nPort);
    }
    else
    {
        // for incoming connections we don't know real user port - use port from hello answer
        bRet = (address2int(m_remote.address()) == np.m_nIP && m_options.m_nPort == np.m_nPort);
    }

    return (bRet);
}

bool peer_connection::has_hash(const md4_hash& hash) const
{
    return (m_hClient == hash);
}

bool peer_connection::operator==(const net_identifier& np) const
{
    return has_network_point(np);
}

bool peer_connection::operator==(const md4_hash& hash) const
{
    return has_hash(hash);
}

void peer_connection::send_message(const std::string& strMessage)
{
    m_ses.m_io_service.post(
        boost::bind(
            &peer_connection::send_throw_meta_order<client_message>,
            self_as<peer_connection>(), client_message(strMessage)));
}

void peer_connection::request_shared_files()
{
    m_ses.m_io_service.post(
        boost::bind(
            &peer_connection::send_throw_meta_order<client_shared_files_request>,
            self_as<peer_connection>(), client_shared_files_request()));
}

void peer_connection::request_shared_directories()
{
    m_ses.m_io_service.post(
        boost::bind(
            &peer_connection::send_throw_meta_order<client_shared_directories_request>,
            self_as<peer_connection>(), client_shared_directories_request()));
}

void peer_connection::request_shared_directory_files(const std::string& strDirectory)
{
    m_ses.m_io_service.post(
        boost::bind(
            &peer_connection::send_throw_meta_order<client_shared_directory_files>,
            self_as<peer_connection>(), client_shared_directory_files(strDirectory)));
}

void peer_connection::request_ismod_directory_files(const md4_hash& hash)
{
    m_ses.m_io_service.post(
        boost::bind(
            &peer_connection::send_throw_meta_order<client_directory_content_request>,
            self_as<peer_connection>(), client_directory_content_request(hash)));
}

void peer_connection::send_deferred()
{
    assert(m_channel_state[upload_channel] == bw_idle);

    while (!m_deferred.empty())
    {
        do_write_message(m_deferred.front());
        m_deferred.pop_front();
    }
}

void peer_connection::fill_send_buffer()
{
    if (m_channel_state[upload_channel] != bw_idle) return;

    int buffer_size_watermark = 10 * BLOCK_SIZE;

    if (m_handshake_complete) { send_deferred(); }

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
        fill_send_buffer();
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

    m_statistics.sent_bytes(r.length, 0);
}

void peer_connection::receive_data(const peer_request& req)
{
    assert(!m_read_in_progress);
    std::pair<peer_request, peer_request> reqs = split_request(req);
    peer_request r = reqs.first;
    peer_request left = reqs.second;
    piece_block block(mk_block(r));

    boost::shared_ptr<transfer> t = m_transfer.lock();
    if (!t) return;

    if (r.length > 0)
    {
        std::vector<pending_block>::iterator b =
            std::find_if(m_download_queue.begin(), m_download_queue.end(), has_block(block));

        if (b == m_download_queue.end())
        {
            ERR("The block we just got from " << m_remote <<
                " : {piece: "<< block.piece_index <<
                ", block: " << block.block_index << ", length: " << r.length
                << "} was not in the request queue");
            return;
        }

        if (!b->buffer)
        {
            if (!allocate_disk_receive_buffer(
                    std::min<size_t>(block_size(block, t->filesize()), DISK_BLOCK_SIZE)))
            {
                ERR("cannot allocate disk receive buffer " << r.length);
                return;
            }

            b->buffer = m_disk_recv_buffer.get();
        }

        boost::asio::async_read(
            *m_socket, boost::asio::buffer(b->buffer + offset_in_block(r), r.length),
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
    // keep ourselves alive in until this function exits in
    // case we disconnect
    boost::intrusive_ptr<peer_connection> me(self_as<peer_connection>());

    m_statistics.received_bytes(bytes_transferred, 0);
    if (error) disconnect(error);
    if (m_disconnecting || is_closed()) return;

    assert(int(bytes_transferred) == r.length);
    assert(m_channel_state[download_channel] == bw_network);
    assert(m_read_in_progress);

    m_read_in_progress = false;
    m_last_receive = time_now();

    boost::shared_ptr<transfer> t = m_transfer.lock();
    if (!t || t->is_seed()) return;

    piece_picker& picker = t->picker();
    piece_manager& fs = t->filesystem();
    piece_block block_finished(mk_block(r));

    std::vector<pending_block>::iterator b
        = std::find_if(m_download_queue.begin(), m_download_queue.end(), has_block(block_finished));

    if (b == m_download_queue.end())
    {
        ERR("The block we just got from " << m_remote <<
            " : {piece: "<< block_finished.piece_index <<
            ", block: " << block_finished.block_index << ", length: " << r.length
            << "} was not in the request queue");
        return;
    }

    // if the block we got is already finished, then ignore it
    if (picker.is_downloaded(block_finished))
    {
        DBG("The block we just got from " << m_remote <<
            " : {piece: "<< block_finished.piece_index <<
            ", block: " << block_finished.block_index << ", length: " << r.length
            << "} is already downloaded");

        //t->add_redundant_bytes(p.length);
        m_download_queue.erase(b);

        request_block();
        send_block_requests();
        return;
    }

    b->complete(mk_range(r));

    if (b->completed())
    {
        disk_buffer_holder holder(m_ses.m_disk_thread, release_disk_receive_buffer());
        fs.async_write(r, holder, boost::bind(&peer_connection::on_disk_write_complete,
                                              self_as<peer_connection>(), _1, _2, r, left, t));
    }

    if (left.length > 0)
        receive_data(left);
    else if (b->completed())
    {
        bool was_finished = picker.is_piece_finished(r.piece);
        picker.mark_as_writing(block_finished, get_peer());
        m_download_queue.erase(b);

        // did we just finish the piece?
        // this means all blocks are either written
        // to disk or are in the disk write cache
        if (picker.is_piece_finished(r.piece) && !was_finished)
        {
            const md4_hash& hash =
                t->filesize() < PIECE_SIZE ? t->hash() : t->hash(r.piece);

            t->async_verify_piece(
                r.piece, hash, boost::bind(&transfer::piece_finished, t, r.piece, _1));
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

        piece_block block_finished(mk_block(r));
        piece_picker& picker = t->picker();
        picker.mark_as_finished(block_finished, get_peer());
    }
}

void peer_connection::write_hello()
{
    DBG("hello ==> " << m_remote);
    const session_settings& settings = m_ses.settings();
    boost::uint32_t nVersion = m_ses.settings().m_version;
    client_hello hello;
    hello.m_nHashLength = MD4_HASH_SIZE;
    hello.m_hClient = settings.user_agent;
    hello.m_network_point.m_nIP = m_ses.m_server_connection->client_id();
    hello.m_network_point.m_nPort = settings.listen_port;
    hello.m_list.add_tag(make_string_tag(settings.client_name, CT_NAME, true));
    hello.m_list.add_tag(make_typed_tag(nVersion, CT_VERSION, true));
    hello.m_server_network_point.m_nIP = address2int(m_ses.server().address());
    hello.m_server_network_point.m_nPort = m_ses.server().port();

    misc_options mo(0);
    mo.m_nUnicodeSupport = 1;
    mo.m_nNoViewSharedFiles = !m_ses.settings().m_show_shared_files;

    misc_options2 mo2(0);
    mo2.set_captcha();
    mo2.set_large_files();
    mo2.set_source_ext2();

    hello.m_list.add_tag(make_typed_tag(mo.generate(), CT_EMULE_MISCOPTIONS1, true));
    hello.m_list.add_tag(make_typed_tag(mo2.generate(), CT_EMULE_MISCOPTIONS2, true));

    do_write(hello);
}

void peer_connection::write_ext_hello()
{
    client_ext_hello ceh;
    ceh.m_nVersion = m_ses.settings().m_version;
    DBG("ext hello {version: " << ceh.m_nVersion << "} ==> " << m_remote);
    do_write(ceh);
}

void peer_connection::write_hello_answer()
{
    // prepare hello answer
    client_hello_answer cha;
    cha.m_hClient               = m_ses.settings().user_agent;
    cha.m_network_point.m_nIP   = m_ses.m_server_connection->client_id();
    cha.m_network_point.m_nPort = m_ses.settings().listen_port;

    boost::uint32_t nVersion = 0x3c;
    boost::uint32_t nUdpPort = 0;

    cha.m_list.add_tag(
        make_string_tag(std::string(m_ses.settings().client_name), CT_NAME, true));
    cha.m_list.add_tag(make_typed_tag(nVersion, CT_VERSION, true));
    cha.m_list.add_tag(make_typed_tag(nUdpPort, CT_EMULE_UDPPORTS, true));
    cha.m_server_network_point.m_nPort = m_ses.settings().server_port;
    cha.m_server_network_point.m_nIP = m_ses.server().address().to_v4().to_ulong();

    DBG("hello answer {client_hash: " << cha.m_hClient <<
        ", client_ip: " << int2ipstr(cha.m_network_point.m_nIP) <<
        ", client_port: " << cha.m_network_point.m_nPort <<
        ", server_ip: " << int2ipstr(cha.m_server_network_point.m_nIP) <<
        ", server_port: " << cha.m_server_network_point.m_nPort << "} ==> " << m_remote);
    do_write(cha);
    m_handshake_complete = true;
}

void peer_connection::write_ext_hello_answer()
{
    client_ext_hello_answer ceha;
    ceha.m_nVersion = m_ses.settings().m_version;  // write only version
    DBG("ext hello answer {version: " << ceha.m_nVersion << "} ==> " << m_remote);
    do_write(ceha);
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
    DBG("hashset {file: " << file_hash << "} ==> " << m_remote);

    client_hashset_answer ha;
    ha.m_hFile = file_hash;
    ha.m_vhParts.m_collection = hash_set;
    do_write(ha);
}

void peer_connection::write_start_upload(const md4_hash& file_hash)
{
    DBG("start upload " << file_hash << " ==> " << m_remote);
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
    if (!t) return;

    client_sending_part_64 sp;
    std::pair<fsize_t, fsize_t> range = mk_range(r);
    sp.m_hFile = t->hash();
    sp.m_begin_offset = range.first;
    sp.m_end_offset = range.second;
    do_write(sp);

    DBG("part " << sp.m_hFile << " [" << sp.m_begin_offset << ", " << sp.m_end_offset << "]"
        << " ==> " << m_remote);
}

void peer_connection::on_hello(const error_code& error)
{
    if (!error)
    {
        DECODE_PACKET(client_hello, hello);
        // store user info
        m_hClient = hello.m_hClient;
        m_options.m_nPort = hello.m_network_point.m_nPort;
        DBG("hello {port: " << m_options.m_nPort << "} <== " << m_remote);
        write_hello_answer();
        write_hello();
    }
    else
    {
        ERR("hello packet received error " << error.message());
    }
}

void peer_connection::on_hello_answer(const error_code& error)
{
    if (!error)
    {
        DECODE_PACKET(client_hello_answer, packet);

        // extract user info from packet
        for (size_t n = 0; n < packet.m_list.count(); ++n)
        {
            const boost::shared_ptr<base_tag> p = packet.m_list[n];

            switch(p->getNameId())
            {
                case CT_NAME:
                    m_options.m_strName = p->asString();
                    break;

                case CT_VERSION:
                    m_options.m_nVersion  = p->asInt();
                    break;

                case ET_MOD_VERSION:
                    if (is_string_tag(p))
                    {
                        m_options.m_strModVersion = p->asString();
                    }
                    else if (is_int_tag(p))
                    {
                        m_options.m_nModVersion = p->asInt();
                    }
                    break;

                case CT_PORT:
                    m_options.m_nPort = p->asInt();
                    break;

                case CT_EMULE_UDPPORTS:
                    m_options.m_nUDPPort = p->asInt() & 0xFFFF;
                    //dwEmuleTags |= 1;
                    break;

                case CT_EMULE_BUDDYIP:
                    // 32 BUDDY IP
                    m_options.m_buddy_point.m_nIP = p->asInt();
                    break;

                case CT_EMULE_BUDDYUDP:
                    m_options.m_buddy_point.m_nPort = p->asInt();
                    break;

                case CT_EMULE_MISCOPTIONS1:
                    m_misc_options.load(p->asInt());
                    break;

                case CT_EMULE_MISCOPTIONS2:
                    m_misc_options2.load(p->asInt());
                    break;

                // Special tag for Compat. Clients Misc options.
                case CT_EMULECOMPAT_OPTIONS:
                    //  1 Operative System Info
                    //  1 Value-based-type int tags (experimental!)
                    m_options.m_bValueBasedTypeTags   = (p->asInt() >> 1*1) & 0x01;
                    m_options.m_bOsInfoSupport        = (p->asInt() >> 1*0) & 0x01;
                    break;

                case CT_EMULE_VERSION:
                    //  8 Compatible Client ID
                    //  7 Mjr Version (Doesn't really matter..)
                    //  7 Min Version (Only need 0-99)
                    //  3 Upd Version (Only need 0-5)
                    //  7 Bld Version (Only need 0-99)
                    m_options.m_nCompatibleClient = (p->asInt() >> 24);
                    m_options.m_nClientVersion = p->asInt() & 0x00ffffff;

                    break;
                default:
                    break;
            }
        }// for

        m_hClient = packet.m_hClient;
        DBG("hello answer {name: " << m_options.m_strName
            << ", port: " << m_options.m_nPort << "} <== " << m_remote);

        m_ses.m_alerts.post_alert_should(
            peer_connected_alert(get_network_point(),
                                 get_connection_hash(),
                                 m_active));

        //write_ext_hello();
        m_handshake_complete = true;
    }
    else
    {
        ERR("hello error " << error.message() << " <== " << m_remote);
    }

    // peer handshake completed
    boost::shared_ptr<transfer> t = m_transfer.lock();

    if (t) write_file_request(t->hash());
    else fill_send_buffer();
}

void peer_connection::on_ext_hello(const error_code& error)
{
    if (!error)
    {
        DECODE_PACKET(client_ext_hello, ext_hello);
        DBG("ext hello {version: " << ext_hello.m_nVersion << "} <== " << m_remote);
        //ext_hello.m_list.dump();
        // store user info
        //m_hClient = hello.m_hClient;
        //m_options.m_nPort = hello.m_network_point.m_nPort;
        //DBG("hello {port: " << m_options.m_nPort << "} <== " << m_remote);
        write_ext_hello_answer();
    }
    else
    {
        ERR("hello packet received error " << error.message());
    }
}

void peer_connection::on_ext_hello_answer(const error_code& error)
{

}

void peer_connection::on_file_request(const error_code& error)
{
    if (!error)
    {
        DECODE_PACKET(client_file_request, fr);
        DBG("file request " << fr.m_hFile << " <== " << m_remote);

        if (attach_to_transfer(fr.m_hFile))
        {
            boost::shared_ptr<transfer> t = m_transfer.lock();
            write_file_status(fr.m_hFile, t->verified_pieces());
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
        DECODE_PACKET(client_file_answer, fa);
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
        DECODE_PACKET(client_file_description, packet);
        DBG("file description " << packet.m_nRating << ", " 
            << packet.m_sComment.m_collection << " <== " << m_remote);
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
        DECODE_PACKET(client_no_file, nf);
        DBG("no file " << nf.m_hFile << " <== " << m_remote);

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
        DECODE_PACKET(client_filestatus_request, fr);
        DBG("file status request " << fr.m_hFile << " <== " << m_remote);

        boost::shared_ptr<transfer> t = m_transfer.lock();
        if (!t) return;

        if (t->hash() == fr.m_hFile)
        {
            write_file_status(t->hash(), t->verified_pieces());
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
        DECODE_PACKET(client_file_status, fs);

        boost::shared_ptr<transfer> t = m_transfer.lock();
        if (!t) return;

        if (fs.m_status.size() == 0)
            fs.m_status.resize(t->num_pieces(), 1);

        DBG("file status answer "<< fs.m_hFile
            << ", [" << bitfield2string(fs.m_status) << "] <== " << m_remote);

        if (t->hash() == fs.m_hFile && t->has_picker())
        {
            m_remote_pieces = fs.m_status;
            t->picker().inc_refcount(fs.m_status);
            if (t->filesize() < PIECE_SIZE)
                write_start_upload(fs.m_hFile);
            else
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
        DECODE_PACKET(client_hashset_request, hr);
        DBG("hash set request " << hr.m_hFile << " <== " << m_remote);

        boost::shared_ptr<transfer> t = m_transfer.lock();
        if (!t) return;

        if (t->hash() == hr.m_hFile)
        {
            write_hashset_answer(t->hash(), t->hashset());
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
        DECODE_PACKET(client_hashset_answer, ha);
        DBG("hash set answer " << ha.m_hFile << " <== " << m_remote);

        boost::shared_ptr<transfer> t = m_transfer.lock();
        if (!t) return;

        if (t->hash() == ha.m_hFile)
        {
            t->hashset(ha.m_vhParts.m_collection);
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
        DECODE_PACKET(client_start_upload, su);
        DBG("start upload " << su.m_hFile << " <== " << m_remote);

        boost::shared_ptr<transfer> t = m_transfer.lock();
        if (!t) return;

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
        DECODE_PACKET(client_queue_ranking, qr);
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

void peer_connection::on_end_download(const error_code& error)
{
    if (!error)
    {
        DECODE_PACKET(client_end_download, ed);
        DBG("end download " << ed.m_hFile << " <== " << m_remote);
    }
    else
    {
        ERR("end download error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_shared_files_request(const error_code& error)
{
    if (!error)
    {
        DBG("peer_connection::on_shared_files_request");        

        if (m_ses.settings().m_show_shared_files)
        {
            client_shared_files_answer sfa;
            // transform transfers to their announces
            std::transform(m_ses.m_transfers.begin(), m_ses.m_transfers.end(), std::back_inserter(sfa.m_files.m_collection), &transfer2sfe);
            // erase empty announces
            sfa.m_files.m_collection.erase(std::remove_if(sfa.m_files.m_collection.begin(), sfa.m_files.m_collection.end(), std::mem_fun_ref(&shared_file_entry::is_empty)), sfa.m_files.m_collection.end());
            DBG("send count: " << sfa.m_files.m_collection.size());
            send_throw_meta_order(sfa);
        }
        else
        {
            send_throw_meta_order(client_shared_files_denied());
        }
    }
    else
    {
        ERR("peer_connection::on_shared_files_answer(" << error.message() << ")");
    }
}

void peer_connection::on_ismod_files_request(const error_code& error)
{
    if (!error)
    {
        DECODE_PACKET(client_directory_content_request, cdcr);
        client_directory_content_result cres;

        // search appropriate transfers and public their filenames
        if (boost::shared_ptr<transfer> p = m_ses.find_transfer(cdcr.m_hash).lock())
        {
            for (aux::session_impl_base::transfer_map::const_iterator i = m_ses.m_transfers.begin(); i != m_ses.m_transfers.end(); ++i)
            {
                transfer& t = *i->second;

                if (t.collectionpath() == p->filepath())
                {
                    cres.m_files.m_collection.push_back(t.getAnnounce());
                }
            }
        }

        send_throw_meta_order(cres);
    }
    else
    {
        ERR("peer_connection::on_shared_files_answer(" << error.message() << ")");
    }

}

void peer_connection::on_shared_files_answer(const error_code& error)
{
    if (!error)
    {
        DECODE_PACKET(client_shared_files_answer, packet);
        m_ses.m_alerts.post_alert_should(
            shared_files_alert(get_network_point(), get_connection_hash(), packet.m_files, false));
    }
    else
    {
        ERR("peer_connection::on_shared_files_answer(" << error.message() << ")");
    }
}

void peer_connection::on_shared_files_denied(const error_code& error)
{
    if (!error)
    {
        DECODE_PACKET(client_shared_files_denied, sfd);
        DBG("shared files access denied <== " << m_remote);
    }
    else
    {
        ERR("shared files denied answer error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_shared_directories_request(const error_code& error)
{
    if (!error)
    {
        DBG("peer_connection::on_shared_directories_request");
        DECODE_PACKET(client_shared_directories_request, sdr);

        if (m_ses.settings().m_show_shared_catalogs)
        {
            client_shared_directories_answer sd;
            std::deque<std::string> dirs;
            std::transform(m_ses.m_transfers.begin(), m_ses.m_transfers.end(), std::back_inserter(dirs), &transfer2catalog);
            std::deque<std::string>::iterator itr = std::unique(dirs.begin(), dirs.end());
            dirs.resize(itr - dirs.begin());
            dirs.erase(std::remove(dirs.begin(), dirs.end(), std::string("")), dirs.end());

            sd.m_dirs.m_collection.resize(dirs.size());

            for (size_t i = 0; i < dirs.size(); ++i)
            {
                sd.m_dirs.m_collection[i].m_collection = dirs[i];
                sd.m_dirs.m_collection[i].m_size = dirs[i].size();
                DBG("send: " << dirs[i]);
            }

            send_throw_meta_order(sd);
        }
    }
    else
    {
        ERR("shared directories answer error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_shared_directories_answer(const error_code& error)
{
    if (!error)
    {
        DECODE_PACKET(client_shared_directories_answer, sd);
        m_ses.m_alerts.post_alert_should(shared_directories_alert(get_network_point(), get_connection_hash(), sd));
    }
    else
    {
        ERR("shared directories answer error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_shared_directory_files_answer(const error_code& error)
{
    if (!error)
    {
        DECODE_PACKET(client_shared_directory_files_answer, sdf);
        m_ses.m_alerts.post_alert_should(shared_directory_files_alert(get_network_point(),
                get_connection_hash(),
                sdf.m_directory.m_collection,
                sdf.m_list));
    }
    else
    {
        ERR("shared directories answer error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_ismod_directory_files(const error_code& error)
{
    if (!error)
    {
        DECODE_PACKET(client_directory_content_result, cdcr);
        m_ses.m_alerts.post_alert_should(shared_files_alert(get_network_point(),
                get_connection_hash(),
                cdcr.m_files,
                false));
    }
    else
    {
        ERR("shared directories answer error " << error.message() << " <== " << m_remote);
    }
}

void peer_connection::on_client_message(const error_code& error)
{
    if (!error)
    {

        DECODE_PACKET(client_message, packet)
        m_ses.m_alerts.post_alert_should(
            peer_message_alert(get_network_point(), get_connection_hash(), packet.m_strMessage));
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
        DECODE_PACKET(client_captcha_request, packet)
        m_ses.m_alerts.post_alert_should(
            peer_captcha_request_alert(get_network_point(), get_connection_hash(), packet.m_captcha));

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
        DECODE_PACKET(client_captcha_result, packet)
        m_ses.m_alerts.post_alert_should(
            peer_captcha_result_alert(get_network_point(), get_connection_hash(), packet.m_captcha_result));
    }
    else
    {
        ERR("on client captcha result error: " << error.message());
    }
}

