
#include "session.hpp"
#include "session_impl.hpp"
#include "transfer.hpp"
#include "peer.hpp"
#include "peer_connection.hpp"
#include "server_connection.hpp"
#include "constants.hpp"
#include "util.hpp"

using namespace libed2k;
namespace ip = boost::asio::ip;


transfer::transfer(aux::session_impl& ses, ip::tcp::endpoint const& net_interface,
                   int seq, add_transfer_params const& p):
    m_ses(ses),
    m_abort(false),
    m_paused(false),
    m_sequential_download(false),
    m_sequence_number(seq),
    m_net_interface(net_interface.address(), 0),
    m_filehash(p.file_hash),
    m_filepath(p.file_path),
    m_filesize(p.file_size),
    m_storage_mode(p.storage_mode),
    m_seed_mode(p.seed_mode),
    m_policy(this),
    m_info(new libtorrent::torrent_info(libtorrent::sha1_hash()))
{
    // TODO: init here
}

transfer::~transfer()
{
    if (!m_connections.empty())
        disconnect_all(errors::transfer_aborted);
}

void transfer::start()
{
    if (!m_seed_mode)
    {
        m_picker.reset(new libtorrent::piece_picker());
        // TODO: resume data, file progress
    }

    init();
}

void transfer::abort()
{
    if (m_abort) return;
    m_abort = true;

    // disconnect all peers and close all
    // files belonging to the torrents
    //disconnect_all(errors::transfer_aborted);

    // post a message to the main thread to destruct
    // the torrent object from there
    if (m_owning_storage.get())
    {
        m_storage->abort_disk_io();
        m_storage->async_release_files(
            boost::bind(&transfer::on_transfer_aborted, shared_from_this(), _1, _2));
    }

    //dequeue_transfer_check();

    //if (m_state == transfer_status::checking_files)
    //    set_state(transfer_status::queued_for_checking);

    m_owning_storage = 0;
}

bool transfer::want_more_peers() const
{
    return !is_finished() && m_policy.num_peers() == 0;
}

void transfer::request_peers()
{
    APP("request peers by hash: " << m_filehash << ", size: " << m_filesize);
    m_ses.m_server_connection->post_sources_request(m_filehash, m_filesize);
}

void transfer::add_peer(const tcp::endpoint& peer)
{
    m_policy.add_peer(peer);
}

bool transfer::connect_to_peer(peer* peerinfo)
{
    tcp::endpoint ep(peerinfo->endpoint);
    boost::shared_ptr<tcp::socket> sock(new tcp::socket(m_ses.m_io_service));

    m_ses.setup_socket_buffers(*sock);

    boost::intrusive_ptr<peer_connection> c(
        new peer_connection(m_ses, shared_from_this(), sock, ep, peerinfo));

    // add the newly connected peer to this transfer's peer list
    m_connections.insert(boost::get_pointer(c));
    m_ses.m_connections.insert(c);
    m_policy.set_connection(peerinfo, c.get());
    c->start();

    int timeout = m_ses.settings().peer_connect_timeout;

    try {
        m_ses.m_half_open.enqueue(
            boost::bind(&peer_connection::connect, c, _1),
            boost::bind(&peer_connection::on_timeout, c),
            libtorrent::seconds(timeout));
    }
    catch (std::exception&)
    {
        std::set<peer_connection*>::iterator i =
            m_connections.find(boost::get_pointer(c));
        if (i != m_connections.end()) m_connections.erase(i);
        c->disconnect(errors::no_error, 1);
        return false;
    }

    return peerinfo->connection;
}

void transfer::remove_peer(peer_connection* p)
{
    // TODO: implement
    DBG("transfer::remove_peer(" << p << ")");
    m_connections.erase(p);
}

bool transfer::want_more_connections() const
{
    return !m_abort && !is_paused() && m_policy.num_connect_candidates() > 0;
}

void transfer::disconnect_all(const error_code& ec)
{
    while (!m_connections.empty())
    {
        peer_connection* p = *m_connections.begin();
        DBG("*** CLOSING CONNECTION: " << ec.message());

        if (p->is_disconnecting())
            m_connections.erase(m_connections.begin());
        else
            p->disconnect(ec);
    }
}

bool transfer::try_connect_peer()
{
    return m_policy.connect_one_peer();
}

void transfer::piece_passed(int index)
{
    bool was_finished = (num_have() == num_pieces());
    we_have(index);
    if (!was_finished && is_finished())
    {
        // transfer finished
        // i.e. all the pieces we're interested in have
        // been downloaded. Release the files (they will open
        // in read only mode if needed)
        finished();
        // if we just became a seed, picker is now invalid, since it
        // is deallocated by the torrent once it starts seeding
    }
}

void transfer::we_have(int index)
{
    //TODO: update progress

    m_picker->we_have(index);
}

int transfer::num_pieces() const
{
    return div_ceil(m_filesize, PIECE_SIZE);
}

// called when torrent is complete (all pieces downloaded)
void transfer::completed()
{
    m_picker.reset();

    //set_state(torrent_status::seeding);
    //if (!m_announcing) return;

    //announce_with_tracker();
}

// called when torrent is finished (all interesting
// pieces have been downloaded)
void transfer::finished()
{
    //TODO: post alert

    //set_state(transfer_status::finished);
    //set_queue_position(-1);

    // we have to call completed() before we start
    // disconnecting peers, since there's an assert
    // to make sure we're cleared the piece picker
    if (is_seed()) completed();

    // disconnect all seeds
    for (std::set<peer_connection*>::iterator i = m_connections.begin();
         i != m_connections.end(); ++i)
    {
        peer_connection* p = *i;
        if (p->upload_only())
        {
            p->disconnect(errors::transfer_finished);
        }
    }

    //if (m_abort) return;

    // we need to keep the object alive during this operation
    m_storage->async_release_files(
        boost::bind(&transfer::on_files_released, shared_from_this(), _1, _2));
}

void transfer::piece_failed(int index)
{
}

void transfer::restore_piece_state(int index)
{
}

bool transfer::is_paused() const
{
    return m_paused || m_ses.is_paused();
}

void transfer::on_files_released(int ret, disk_io_job const& j)
{
    // do nothing
}

void transfer::on_transfer_aborted(int ret, disk_io_job const& j)
{
    // the transfer should be completely shut down now, and the
    // destructor has to be called from the main thread
}

void transfer::init()
{
    // the shared_from_this() will create an intentional
    // cycle of ownership, see the hpp file for description.
    m_owning_storage = new libtorrent::piece_manager(
        shared_from_this(), m_info, m_filepath.parent_path(), m_ses.m_filepool,
        m_ses.m_disk_thread, libtorrent::default_storage_constructor,
        static_cast<libtorrent::storage_mode_t>(m_storage_mode));
    m_storage = m_owning_storage.get();

    if (has_picker())
    {
        int blocks_per_piece = 1;
        int blocks_in_last_piece = 1;
        m_picker->init(blocks_per_piece, blocks_in_last_piece, num_pieces());
    }

    // TODO: checking resume data

}

void transfer::second_tick()
{
    if (want_more_peers()) request_peers();

    for (std::set<peer_connection*>::iterator i = m_connections.begin();
         i != m_connections.end(); ++i)
    {
        peer_connection* p = *i;

        try
        {
            p->second_tick();
        }
        catch (std::exception& e)
        {
            DBG("**ERROR**: " << e.what());
            p->disconnect(errors::no_error, 1);
        }
    }
}

void transfer::async_verify_piece(int piece_index, boost::function<void(int)> const&)
{
    //TODO: piece verification
}

// passed_hash_check
//  0: success, piece passed check
// -1: disk failure
// -2: piece failed check
void transfer::piece_finished(int index, int passed_hash_check)
{
    // even though the piece passed the hash-check
    // it might still have failed being written to disk
    // if so, piece_picker::write_failed() has been
    // called, and the piece is no longer finished.
    // in this case, we have to ignore the fact that
    // it passed the check
    if (!m_picker->is_piece_finished(index)) return;

    if (passed_hash_check == 0)
    {
        // the following call may cause picker to become invalid
        // in case we just became a seed
        piece_passed(index);
    }
    else if (passed_hash_check == -2)
    {
        // piece_failed() will restore the piece
        piece_failed(index);
    }
    else
    {
        m_picker->restore_piece(index);
        restore_piece_state(index);
    }
}

void transfer::announce()
{
    // announce always now
    shared_file_entry entry = getAnnounce();
    m_ses.announce(entry);
}

shared_file_entry transfer::getAnnounce() const
{
    shared_file_entry entry;
    // generate file entry from transfer here

    return entry;
}

void transfer::send_server_request(const server_request& req)
{
}

void transfer::on_server_response(const server_request& req, const server_response& res)
{
}

void transfer::on_server_request_timed_out(const server_request& req)
{
}

void transfer::on_server_request_error(const server_request& req, int response_code,
                                       const std::string& str, int retry_interval)
{
}
