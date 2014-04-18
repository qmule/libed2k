#include "libed2k/version.hpp"
#include "libed2k/session.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/transfer.hpp"
#include "libed2k/peer.hpp"
#include "libed2k/peer_connection.hpp"
#include "libed2k/server_connection.hpp"
#include "libed2k/constants.hpp"
#include "libed2k/util.hpp"
#include "libed2k/file.hpp"
#include "libed2k/alert_types.hpp"

namespace libed2k
{
    /** fake constructor */
    transfer::transfer(aux::session_impl& ses, const std::vector<peer_entry>& pl,
                       const md4_hash& hash, const std::string& filepath, size_type size):
        m_ses(ses),
        m_save_path(parent_path(filepath)),
        m_complete(-1),
        m_incomplete(-1),
        m_policy(this),
        m_info(new transfer_info(hash, filename(filepath), size)),
        m_minute_timer(minutes(1), min_time())
    {}

    transfer::transfer(aux::session_impl& ses, ip::tcp::endpoint const& net_interface,
                       int seq, add_transfer_params const& p):
        m_ses(ses),
        m_announced(false),
        m_abort(false),
        m_paused(false),
        m_sequential_download(false),
        m_sequence_number(seq),
        m_net_interface(net_interface.address(), 0),
        m_save_path(parent_path(p.file_path)),
        m_upload_mode_time(0),
        m_storage_mode(p.storage_mode),
        m_state(transfer_status::checking_resume_data),
        m_seed_mode(p.seed_mode),
        m_upload_mode(false),
        m_eager_mode(false),
        m_auto_managed(false),
        m_complete(p.num_complete_sources),
        m_incomplete(p.num_incomplete_sources),
        m_policy(this),
        m_info(new transfer_info(p.file_hash, filename(p.file_path), p.file_size, p.piece_hashses)),
        m_accepted(p.accepted),
        m_requested(p.requested),
        m_transferred(p.transferred),
        m_priority(p.priority),
        m_total_uploaded(0),
        m_total_downloaded(0),
        m_queued_for_checking(false),
        m_progress_ppm(0),
        m_total_failed_bytes(0),
        m_total_redundant_bytes(0),
        m_minute_timer(minutes(1), min_time()),
        m_need_save_resume_data(true),
        m_last_active(0)
    {
        if (p.resume_data) m_resume_data.swap(*p.resume_data);
    }

    transfer::~transfer()
    {
        if (!m_connections.empty())
            disconnect_all(errors::transfer_aborted);
    }

    const md4_hash& transfer::hash() const { return m_info->info_hash(); }
    size_type transfer::size() const { return m_info->file_at(0).size; }
    const std::string& transfer::name() const { return m_info->name(); }
    const std::string& transfer::save_path() const { return m_save_path; }
    std::string transfer::file_path() const { return combine_path(save_path(), name()); }
    const std::vector<md4_hash>& transfer::piece_hashses() const { return m_info->piece_hashses(); }
    void transfer::piece_hashses(const std::vector<md4_hash>& hs) {
        LIBED2K_ASSERT(hs.size() > 0);
        m_info->piece_hashses(hs);
    }

    const md4_hash& transfer::hash_for_piece(size_t piece) const
    {
        return m_info->hash_for_piece(piece);
    }

    add_transfer_params transfer::params() const
    {
        add_transfer_params res;
        res.file_hash = hash();
        res.file_path = file_path();
        res.file_size = size();
        res.piece_hashses = piece_hashses();
        //res.resume_data = &m_resume_data;
        res.storage_mode = m_storage_mode;
        //res.duplicate_is_error = ???
        res.seed_mode = m_seed_mode;
        res.num_complete_sources = m_complete;
        res.num_incomplete_sources = m_incomplete;
        res.accepted = m_accepted;
        res.requested = m_requested;
        res.transferred = m_transferred;
        res.priority = m_priority;
        return res;
    }

    transfer_handle transfer::handle()
    {
        return transfer_handle(shared_from_this());
    }

    void transfer::start()
    {
        LIBED2K_ASSERT(!m_picker);

        if (!m_seed_mode)
        {
            m_picker.reset(new piece_picker());
            // TODO: file progress

            error_code ec;
            if (!m_resume_data.empty() &&
                lazy_bdecode(&m_resume_data[0], &m_resume_data[0] + m_resume_data.size(),
                             m_resume_entry, ec) != 0)
            {
                ERR("fast resume parse error: "
                    "{file: " << name() << ", error: " << ec.message() << "}");
                std::vector<char>().swap(m_resume_data);
                m_ses.m_alerts.post_alert_should(
                    fastresume_rejected_alert(handle(), errors::fast_resume_parse_error));
            }
        }

        init();
    }

    void transfer::abort()
    {
        if (m_abort) return;
        m_abort = true;

        DBG("abort transfer {hash: " << hash() << "}");

        // disconnect all peers and close all
        // files belonging to the transfer
        disconnect_all(errors::transfer_aborted);

        // post a message to the main thread to destruct
        // the transfer object from there
        if (m_owning_storage.get())
        {
            m_storage->abort_disk_io();
            m_storage->async_release_files(
                boost::bind(&transfer::on_transfer_aborted, shared_from_this(), _1, _2));
        }

        dequeue_transfer_check();

        if (m_state == transfer_status::checking_files)
            set_state(transfer_status::queued_for_checking);

        m_owning_storage = 0;
    }

    void transfer::set_state(transfer_status::state_t s)
    {
        if (m_state == s) return;
        m_ses.m_alerts.post_alert_should(state_changed_alert(handle(), s, m_state));
        m_state = s;

        if (s != transfer_status::seeding)
            activate(true);
    }

    const session_settings& transfer::settings() const
    {
        return m_ses.settings();
    }

    bool transfer::valid_metadata() const
    {
        return m_info->is_valid();
    }

    bool transfer::want_more_peers() const
    {
        return
            /*m_connections.size() < m_max_connections &&*/
            !is_paused() &&
            ((m_state != transfer_status::checking_files &&
              m_state != transfer_status::checking_resume_data &&
              m_state != transfer_status::queued_for_checking) || !valid_metadata()) &&
            m_policy.num_connect_candidates() > 0 && !m_abort /*&&
            (m_ses.settings().seeding_outgoing_connections ||
             (m_state != transfer_status::seeding && m_state != transfer_status::finished))*/;
    }

    void transfer::request_peers()
    {
        APP("request peers by hash: " << hash() << ", size: " << size());
        m_ses.m_server_connection->post_sources_request(hash(), size());
    }

    void transfer::add_peer(const tcp::endpoint& peer, int source)
    {
        m_policy.add_peer(peer, source, 0);

        state_updated();
    }

    bool transfer::connect_to_peer(peer* peerinfo)
    {
        LIBED2K_ASSERT(peerinfo);
        LIBED2K_ASSERT(peerinfo->connection == 0);
        LIBED2K_ASSERT(peerinfo->next_connect >= m_ses.session_time());

        peerinfo->last_connected = m_ses.session_time();
        peerinfo->next_connect = 0;

        tcp::endpoint ep(peerinfo->endpoint);
        LIBED2K_ASSERT((m_ses.m_ip_filter.access(peerinfo->address()) & ip_filter::blocked) == 0);

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

        try
        {
            m_ses.m_half_open.enqueue(
                boost::bind(&peer_connection::connect, c, _1),
                boost::bind(&peer_connection::on_timeout, c),
                libed2k::seconds(timeout));
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

    bool transfer::attach_peer(peer_connection* p)
    {
        LIBED2K_ASSERT(!p->has_transfer());

        if (m_ses.m_ip_filter.access(p->remote().address()) & ip_filter::blocked)
        {
            m_ses.m_alerts.post_alert_should(peer_blocked_alert(handle(), p->remote().address()));
            p->disconnect(errors::banned_by_ip_filter);
            return false;
        }

        if (m_state == transfer_status::queued_for_checking ||
            m_state == transfer_status::checking_files ||
            m_state == transfer_status::checking_resume_data)
        {
            p->disconnect(errors::transfer_not_ready);
            return false;
        }

        if (m_ses.m_connections.find(p) == m_ses.m_connections.end())
        {
            p->disconnect(errors::peer_not_constructed);
            return false;
        }
        if (m_ses.is_aborted())
        {
            p->disconnect(errors::session_closing);
            return false;
        }
        if (!m_policy.new_connection(*p, m_ses.session_time()))
            return false;

        LIBED2K_ASSERT(m_connections.find(p) == m_connections.end());
        m_connections.insert(p);
        activate(true);

        return true;
    }

    void transfer::remove_peer(peer_connection* c)
    {
        std::set<peer_connection*>::iterator i = m_connections.find(c);
        if (i == m_connections.end())
        {
            LIBED2K_ASSERT(false);
            return;
        }

        if (ready_for_connections())
        {
            LIBED2K_ASSERT(c->get_transfer().lock().get() == this);

            if (c->is_seed())
            {
                if (m_picker.get())
                {
                    m_picker->dec_refcount_all();
                }
            }
            else
            {
                if (m_picker.get())
                {
                    const bitfield& pieces = c->remote_pieces();
                    if (pieces.size() > 0)
                        m_picker->dec_refcount(pieces);
                }
            }
        }

        m_policy.connection_closed(*c, m_ses.session_time());
        c->set_peer(0);
        m_connections.erase(c);
    }

    void transfer::get_peer_info(std::vector<peer_info>& infos)
    {
        infos.clear();
        for (std::set<peer_connection*>::iterator i = m_connections.begin();
             i != m_connections.end(); ++i)
        {
            peer_connection* peer = *i;

            // incoming peers that haven't finished the handshake should
            // not be included in this list
            if (peer->get_transfer().expired()) continue;

            infos.push_back(peer_info());
            peer_info& p = infos.back();

            peer->get_peer_info(p);
            // peer country
        }
    }

    void transfer::disconnect_all(const error_code& ec)
    {
        while (!m_connections.empty())
        {
            peer_connection* p = *m_connections.begin();

            if (p->is_disconnecting())
                m_connections.erase(m_connections.begin());
            else
                p->disconnect(ec);
        }
    }

    int transfer::disconnect_peers(int num, error_code const& ec)
    {
        // FIXME - implement method
        return 0;
    }

    bool transfer::try_connect_peer()
    {
        LIBED2K_ASSERT(want_more_peers());
        return m_policy.connect_one_peer(m_ses.session_time());
    }

    void transfer::piece_passed(int index)
    {
        bool was_finished = (num_have() == num_pieces());
        we_have(index);
        m_need_save_resume_data = true;

        if (!was_finished && is_finished())
        {
            // transfer finished
            // i.e. all the pieces we're interested in have
            // been downloaded. Release the files (they will open
            // in read only mode if needed)
            finished();
            // if we just became a seed, picker is now invalid, since it
            // is deallocated by the transfer once it starts seeding
        }
    }

    bitfield transfer::have_pieces() const
    {
        int nPieces = num_pieces();
        bitfield res(nPieces, false);

        for (int p = 0; p < nPieces; ++p)
            if (have_piece(p))
                res.set_bit(p);

        return res;
    }

    void transfer::we_have(int index)
    {
        //TODO: update progress
        m_picker->we_have(index);
    }

    size_t transfer::num_pieces() const
    {
        return m_info->num_pieces();
    }

    size_t transfer::num_free_blocks() const
    {
        size_t res = 0;
        int pieces = int(num_pieces());

        if (has_picker())
        {
            for (int p = 0; p < pieces; ++p)
            {
                piece_picker::downloading_piece dp;
                m_picker->piece_info(p, dp);
                res += (m_picker->blocks_in_piece(p) - dp.finished - dp.writing - dp.requested);
            }
        }

        return res;
    }

    // called when transfer is complete (all pieces downloaded)
    void transfer::completed()
    {
        m_picker.reset();
        set_state(transfer_status::seeding);
    }

    // called when transfer is finished (all interesting pieces have been downloaded)
    void transfer::finished()
    {
        DBG("file transfer '" << hash() << ", " << name() << "' completed");
        set_state(transfer_status::finished);
        //set_queue_position(-1);

        // we have to call completed() before we start
        // disconnecting peers, since there's an assert
        // to make sure we're cleared the piece picker
        if (is_seed()) completed();

        // disconnect all seeds
        std::vector<peer_connection*> seeds;
        for (std::set<peer_connection*>::iterator i = m_connections.begin();
             i != m_connections.end(); ++i)
        {
            peer_connection* p = *i;
            if (p->remote_pieces().count() == int(num_have()))
                seeds.push_back(p);
        }

        m_ses.m_alerts.post_alert_should(finished_transfer_alert(handle(), seeds.size() > 0));
        std::for_each(seeds.begin(), seeds.end(),
                      boost::bind(&peer_connection::disconnect, _1, errors::transfer_finished, 0));

        if (m_abort) return;

        // we need to keep the object alive during this operation
        m_storage->async_release_files(
            boost::bind(&transfer::on_files_released, shared_from_this(), _1, _2));
    }

    void transfer::add_stats(const stat& s)
    {
        // these stats are propagated to the session
        // stats the next time second_tick is called
        m_stat += s;
    }

    void transfer::set_upload_mode(bool b)
    {
        if (b == m_upload_mode) return;

        m_upload_mode = b;

        state_updated();
        //send_upload_only();

        if (m_upload_mode)
        {
            // clear request queues of all peers
            for (std::set<peer_connection*>::iterator i = m_connections.begin(),
                     end(m_connections.end()); i != end; ++i)
            {
                peer_connection* p = (*i);
                p->cancel_all_requests();
            }
            // this is used to try leaving upload only mode periodically
            m_upload_mode_time = 0;
        }
        else
        {
            // reset last_connected, to force fast reconnect after leaving upload mode
            for (policy::peers_t::iterator i = m_policy.begin_peer(), end(m_policy.end_peer()); i != end; ++i)
            {
                (*i)->last_connected = 0;
            }

            // send_block_requests on all peers
            for (std::set<peer_connection*>::iterator i = m_connections.begin(),
                     end(m_connections.end()); i != end; ++i)
            {
                peer_connection* p = (*i);
                p->send_block_requests();
            }
        }
    }

    void transfer::pause()
    {
        if (m_paused) return;
        m_paused = true;

        // we need to save this new state
        m_need_save_resume_data = true;

        if (!m_ses.is_paused())
            do_pause();
    }

    void transfer::do_pause()
    {
        if (!is_paused()) return;
        DBG("pause transfer {hash: " << hash() << "}");

        state_updated();

        // this will make the storage close all
        // files and flush all cached data
        if (m_owning_storage.get())
        {
            LIBED2K_ASSERT(m_storage);
            m_storage->async_release_files(
                boost::bind(&transfer::on_transfer_paused, shared_from_this(), _1, _2));
            m_storage->async_clear_read_cache();
        }
        else
        {
            m_ses.m_alerts.post_alert_should(paused_transfer_alert(handle()));
        }

        disconnect_all(errors::transfer_paused);

        if (m_queued_for_checking && !should_check_file())
        {
            // stop checking
            m_storage->abort_disk_io();
            dequeue_transfer_check();
            set_state(transfer_status::queued_for_checking);
            LIBED2K_ASSERT(!m_queued_for_checking);
        }
    }

    void transfer::resume()
    {
        if (!m_paused) return;
        m_paused = false;

        // we need to save this new state
        m_need_save_resume_data = true;

        do_resume();
    }

    void transfer::do_resume()
    {
        if (is_paused()) return;
        DBG("resume transfer {hash: " << hash() << "}");

        m_ses.m_alerts.post_alert_should(resumed_transfer_alert(handle()));
        state_updated();
        if (!m_queued_for_checking && should_check_file())
            queue_transfer_check();
    }

    void transfer::set_upload_limit(int limit)
    {
        LIBED2K_ASSERT(limit >= -1);
        if (limit <= 0) limit = 0;
        if (m_bandwidth_channel[peer_connection::upload_channel].throttle() != limit)
            state_updated();
        m_bandwidth_channel[peer_connection::upload_channel].throttle(limit);
    }

    int transfer::upload_limit() const
    {
        int limit = m_bandwidth_channel[peer_connection::upload_channel].throttle();
        if (limit == (std::numeric_limits<int>::max)()) limit = -1;
        return limit;
    }

    void transfer::set_download_limit(int limit)
    {
        LIBED2K_ASSERT(limit >= -1);
        if (limit <= 0) limit = 0;
        if (m_bandwidth_channel[peer_connection::download_channel].throttle() != limit)
            state_updated();
        m_bandwidth_channel[peer_connection::download_channel].throttle(limit);
    }

    int transfer::download_limit() const
    {
        int limit = m_bandwidth_channel[peer_connection::download_channel].throttle();
        if (limit == (std::numeric_limits<int>::max)()) limit = -1;
        return limit;
    }

    storage_interface* transfer::get_storage()
    {
        if (!m_owning_storage) return 0;
        return m_owning_storage->get_storage_impl();
    }

    void transfer::move_storage(const std::string& save_path)
    {
        if (m_owning_storage.get())
        {
            m_owning_storage->async_move_storage(
                save_path, boost::bind(&transfer::on_storage_moved, shared_from_this(), _1, _2));
        }
        else
        {
            m_ses.m_alerts.post_alert_should(storage_moved_alert(handle(), save_path));
            m_save_path = save_path;
        }
    }

    bool transfer::rename_file(const std::string& name)
    {
        DBG("renaming file in transfer {hash: " << hash() <<
            ", from: " << transfer::name() << ", to: " << name << "}");
        if (!m_owning_storage.get()) return false;

        m_owning_storage->async_rename_file(
            0, name, boost::bind(&transfer::on_file_renamed, shared_from_this(), _1, _2));
        return true;
    }

    void transfer::delete_files()
    {
        DBG("deleting file in transfer {hash: " << hash() << ", files: " << name() << "}");
        disconnect_all(errors::transfer_removed);

        if (m_owning_storage.get())
        {
            LIBED2K_ASSERT(m_storage);
            m_storage->async_delete_files(
                boost::bind(&transfer::on_files_deleted, shared_from_this(), _1, _2));
        }
    }

    void transfer::piece_availability(std::vector<int>& avail) const
    {
        if (is_seed())
        {
            avail.clear();
            return;
        }

        m_picker->get_availability(avail);
    }

    void transfer::set_piece_priority(int index, int priority)
    {
        if (is_seed()) return;

        // this call is only valid on transfers with metadata
        LIBED2K_ASSERT(m_picker.get());
        LIBED2K_ASSERT(index >= 0);
        LIBED2K_ASSERT(index < int(num_pieces()));
        if (index < 0 || index >= int(num_pieces())) return;

        if (m_picker->set_piece_priority(index, priority))
            m_need_save_resume_data = true;
    }

    int transfer::piece_priority(int index) const
    {
        if (is_seed()) return 1;

        // this call is only valid on transfers with metadata
        LIBED2K_ASSERT(m_picker.get());
        LIBED2K_ASSERT(index >= 0);
        LIBED2K_ASSERT(index < int(num_pieces()));
        if (index < 0 || index >= int(num_pieces())) return 0;

        return m_picker->piece_priority(index);
    }

	void transfer::piece_priorities(std::vector<int>* pieces) const
    {
        if (is_seed())
        {
            pieces->clear();
            pieces->resize(num_pieces(), 1);
            return;
        }

        LIBED2K_ASSERT(m_picker.get());
        m_picker->piece_priorities(*pieces);
    }

    int transfer::priority() const { return m_priority; }
    void transfer::set_priority(int prio)
    {
        LIBED2K_ASSERT(prio <= 255 && prio >= 0);
        if (prio > 255) prio = 255;
        else if (prio < 0) prio = 0;
        m_priority = prio;
        state_updated();
    }

    void transfer::set_sequential_download(bool sd) { m_sequential_download = sd; }

    void transfer::piece_failed(int index)
    {
        LIBED2K_ASSERT(m_storage);
        LIBED2K_ASSERT(m_storage->refcount() > 0);
        LIBED2K_ASSERT(m_picker);
        LIBED2K_ASSERT(index >= 0);
        LIBED2K_ASSERT(index < int(num_pieces()));

        m_ses.m_alerts.post_alert_should(hash_failed_alert(handle(), index));

        // increase the total amount of failed bytes
        add_failed_bytes(PIECE_SIZE);

        // TODO:
        // decrease the trust point of all peers that sent
        // parts of this piece.
        // first, build a set of all peers that participated

        // we have to let the piece_picker know that
        // this piece failed the check as it can restore it
        // and mark it as being interesting for download
        m_picker->restore_piece(index);

        // we might still have outstanding requests to this
        // piece that hasn't been received yet. If this is the
        // case, we need to re-open the piece and mark any
        // blocks we're still waiting for as requested
        restore_piece_state(index);

        LIBED2K_ASSERT(m_storage);
        LIBED2K_ASSERT(m_picker->have_piece(index) == false);
    }

    void transfer::restore_piece_state(int index)
    {
        LIBED2K_ASSERT(has_picker());
        for (std::set<peer_connection*>::const_iterator i = m_connections.begin();
             i != m_connections.end(); ++i)
        {
            peer_connection* p = *i;
            std::vector<pending_block> const& dq = p->download_queue();
            std::vector<pending_block> const& rq = p->request_queue();
            for (std::vector<pending_block>::const_iterator k = dq.begin(), end(dq.end());
                 k != end; ++k)
            {
                if (k->timed_out || k->not_wanted) continue;
                if (int(k->block.piece_index) != index) continue;
                m_picker->mark_as_downloading(
                    k->block, p->get_peer(), (piece_picker::piece_state_t)p->peer_speed());
            }
            for (std::vector<pending_block>::const_iterator k = rq.begin(), end(rq.end());
                 k != end; ++k)
            {
                if (int(k->block.piece_index) != index) continue;
                m_picker->mark_as_downloading(
                    k->block, p->get_peer(), (piece_picker::piece_state_t)p->peer_speed());
            }
        }
    }

    int transfer::num_peers() const
    {
        return (int)std::count_if(
            m_connections.begin(), m_connections.end(),
            !boost::bind(&peer_connection::is_connecting, _1));
    }

    int transfer::num_seeds() const
    {
        return (int)std::count_if(
            m_connections.begin(), m_connections.end(),
            boost::bind(&peer_connection::is_seed, _1));
    }

    bool transfer::is_paused() const
    {
        return m_paused || m_ses.is_paused();
    }

    transfer_status transfer::status() const
    {
        transfer_status st;

        st.seed_mode = m_seed_mode;
        st.upload_mode = m_upload_mode;
        st.paused = m_paused;

        bytes_done(st);

        st.all_time_upload = m_total_uploaded;
        st.all_time_download = m_total_downloaded;

        st.num_complete = m_complete;
        st.num_incomplete = m_incomplete;

        // payload transfer
        st.total_payload_download = m_stat.total_payload_download();
        st.total_payload_upload = m_stat.total_payload_upload();

        // total transfer
        st.total_download = m_stat.total_payload_download() +
            m_stat.total_protocol_download();
        st.total_upload = m_stat.total_payload_upload() +
            m_stat.total_protocol_upload();

        // failed bytes
        st.total_failed_bytes = m_total_failed_bytes;
        st.total_redundant_bytes = m_total_redundant_bytes;

        // transfer rate
        st.download_rate = m_stat.download_rate();
        st.upload_rate = m_stat.upload_rate();
        st.download_payload_rate = m_stat.download_payload_rate();
        st.upload_payload_rate = m_stat.upload_payload_rate();

        st.priority = m_priority;

        st.num_seeds = num_seeds();
        st.num_peers = num_peers();

        st.list_peers = m_policy.num_peers();
        //st.list_seeds = m_policy.num_seeds();
        //st.connect_candidates = m_policy.num_connect_candidates();
        st.num_connections = m_connections.size();
        st.connections_limit = 0; // TODO: m_max_connections;

        st.state = m_state;

        if (m_state == transfer_status::checking_files)
        {
            st.progress_ppm = m_progress_ppm;
        }
        else if (st.total_wanted == 0)
        {
            st.progress_ppm = 1000000;
            st.progress = 1.f;
        }
        else
        {
            st.progress_ppm = st.total_wanted_done * 1000000 / st.total_wanted;
        }

        if (has_picker())
        {
            st.sparse_regions = m_picker->sparse_regions();
            int num_pieces = m_picker->num_pieces();
            st.pieces.resize(num_pieces, false);
            for (int i = 0; i < num_pieces; ++i)
                if (m_picker->have_piece(i)) st.pieces.set_bit(i);
        }
        else if (is_seed())
        {
            int num_pieces = this->num_pieces();
            st.pieces.resize(num_pieces, true);
        }
        st.num_pieces = num_have();

        return st;
    }

    void transfer::state_updated()
    {
    }

    // fills in total_wanted, total_wanted_done and total_done
    void transfer::bytes_done(transfer_status& st) const
    {
        st.total_wanted = size();
        st.total_done = std::min<size_type>(num_have() * PIECE_SIZE, st.total_wanted);
        st.total_wanted_done = st.total_done;

        if (!m_picker) return;

        const int last_piece = num_pieces() - 1;

        // if we have the last piece, we have to correct
        // the amount we have, since the first calculation
        // assumed all pieces were of equal size
        if (m_picker->have_piece(last_piece))
        {
            int corr = size() % PIECE_SIZE - PIECE_SIZE;
            LIBED2K_ASSERT(corr <= 0);
            LIBED2K_ASSERT(corr > -int(PIECE_SIZE));
            st.total_done += corr;
            if (m_picker->piece_priority(last_piece) != 0)
            {
                LIBED2K_ASSERT(st.total_wanted_done >= int(PIECE_SIZE));
                st.total_wanted_done += corr;
            }
        }
        LIBED2K_ASSERT(st.total_wanted >= st.total_wanted_done);

        const std::vector<piece_picker::downloading_piece>& dl_queue =
            m_picker->get_download_queue();

        const int blocks_per_piece = div_ceil(PIECE_SIZE, BLOCK_SIZE);

        // look at all unfinished pieces and add the completed
        // blocks to our 'done' counter
        for (std::vector<piece_picker::downloading_piece>::const_iterator i =
                 dl_queue.begin(); i != dl_queue.end(); ++i)
        {
            int corr = 0;
            int index = i->index;
            // completed pieces are already accounted for
            if (m_picker->have_piece(index)) continue;
            LIBED2K_ASSERT(i->finished <= m_picker->blocks_in_piece(index));

            for (int j = 0; j < blocks_per_piece; ++j)
            {
                if (i->info[j].state == piece_picker::block_info::state_writing ||
                    i->info[j].state == piece_picker::block_info::state_finished)
                {
                    corr += block_bytes_wanted(piece_block(index, j));
                }
                LIBED2K_ASSERT(corr >= 0);
                //LIBED2K_ASSERT(index != last_piece || j < m_picker->blocks_in_last_piece() ||
                //       i->info[j].state != piece_picker::block_info::state_finished);
            }

            st.total_done += corr;
            if (m_picker->piece_priority(index) > 0)
                st.total_wanted_done += corr;
        }

        std::map<piece_block, int> downloading_piece;
        for (std::set<peer_connection*>::const_iterator i = m_connections.begin();
             i != m_connections.end(); ++i)
        {
            peer_connection* pc = *i;
            boost::optional<piece_block_progress> p = pc->downloading_piece_progress();
            if (!p) continue;

            if (m_picker->have_piece(p->piece_index))
                continue;

            piece_block block(p->piece_index, p->block_index);
            if (m_picker->is_finished(block))
                continue;

            std::map<piece_block, int>::iterator dp = downloading_piece.find(block);
            if (dp != downloading_piece.end())
            {
                if (dp->second < p->bytes_downloaded)
                    dp->second = p->bytes_downloaded;
            }
            else
            {
                downloading_piece[block] = p->bytes_downloaded;
            }
        }

        for (std::map<piece_block, int>::iterator i = downloading_piece.begin();
             i != downloading_piece.end(); ++i)
        {
            int done = std::min(block_bytes_wanted(i->first), i->second);
            st.total_done += done;
            if (m_picker->piece_priority(i->first.piece_index) != 0)
                st.total_wanted_done += done;
        }
    }

    void transfer::add_failed_bytes(int b)
    {
        LIBED2K_ASSERT(b > 0);
        m_total_failed_bytes += b;
        m_ses.add_failed_bytes(b);
    }

    void transfer::on_files_released(int ret, disk_io_job const& j)
    {
        // do nothing
    }

    void transfer::on_files_deleted(int ret, disk_io_job const& j)
    {
        boost::mutex::scoped_lock l(m_ses.m_mutex);

        if (ret != 0)
            m_ses.m_alerts.post_alert_should(delete_failed_transfer_alert(handle(), j.error));
        else
            m_ses.m_alerts.post_alert_should(deleted_file_alert(handle(), hash()));
    }

    void transfer::on_file_renamed(int ret, disk_io_job const& j)
    {
        boost::mutex::scoped_lock l(m_ses.m_mutex);

        LIBED2K_ASSERT(j.piece == 0);

        if (ret == 0)
        {
            DBG("file successfully renamed {hash: " << hash() << ", to: " << j.str << "}");
            m_ses.m_alerts.post_alert_should(file_renamed_alert(handle(), j.str));
        }
        else
        {
            DBG("file rename failed {hash: " << hash() << ", err: " << j.error << "}");
            m_ses.m_alerts.post_alert_should(
                file_rename_failed_alert(handle(), j.error));
        }
    }

    void transfer::on_storage_moved(int ret, disk_io_job const& j)
    {
        boost::mutex::scoped_lock l(m_ses.m_mutex);

        if (ret == 0)
        {
            DBG("storage successfully moved {hash: " << hash() << ", to: " << j.str << "}");
            m_ses.m_alerts.post_alert_should(storage_moved_alert(handle(), j.str));
            m_save_path = j.str;
        }
        else
        {
            DBG("storage move failed {hash: " << hash() << ", err: " << j.error << "}");
            m_ses.m_alerts.post_alert_should(storage_moved_failed_alert(handle(), j.error));
        }
    }

    void transfer::on_transfer_aborted(int ret, disk_io_job const& j)
    {
        // the transfer should be completely shut down now, and the
        // destructor has to be called from the main thread
    }

    void transfer::on_transfer_paused(int ret, disk_io_job const& j)
    {
        boost::mutex::scoped_lock l(m_ses.m_mutex);

        m_ses.m_alerts.post_alert_should(paused_transfer_alert(handle()));
    }

    void transfer::init()
    {
        DBG("init transfer: {hash: " << hash() << ", file: " << name() << "}");

        // we have only one file with normal priority
        std::vector<boost::uint8_t> file_prio;
        file_prio.push_back(1);

        // the shared_from_this() will create an intentional
        // cycle of ownership, see the hpp file for description.
        m_owning_storage = new piece_manager(
            shared_from_this(), m_info, m_save_path, m_ses.m_filepool,
            m_ses.m_disk_thread, default_storage_constructor, m_storage_mode, file_prio);
        m_storage = m_owning_storage.get();

        if (has_picker())
        {
            int blocks_per_piece = div_ceil(PIECE_SIZE, BLOCK_SIZE);
            int blocks_in_last_piece = div_ceil(size() % PIECE_SIZE, BLOCK_SIZE);
            m_picker->init(blocks_per_piece, blocks_in_last_piece, num_pieces());
        }

        if (!m_seed_mode)
        {
            set_state(transfer_status::checking_resume_data);

            if (m_resume_entry.type() == lazy_entry::dict_t)
            {
                DBG("read resume data: {hash: " << hash() << ", file: " << name() << "}");
                int ev = 0;
                if (m_resume_entry.dict_find_string_value("file-format") != "libed2k resume file")
                    ev = errors::invalid_file_tag;

                std::string info_hash = m_resume_entry.dict_find_string_value("transfer-hash");

                if (!ev && info_hash.empty())
                    ev = errors::missing_transfer_hash;

                if (!ev && (md4_hash::fromString(info_hash) != hash()))
                    ev = errors::mismatching_transfer_hash;

                if (ev)
                {
                    std::vector<char>().swap(m_resume_data);
                    lazy_entry().swap(m_resume_entry);
                    m_ses.m_alerts.post_alert_should(
                        fastresume_rejected_alert(handle(), error_code(ev,  get_libed2k_category())));
                }
                else
                {
                    read_resume_data(m_resume_entry);
                    m_need_save_resume_data = false;
                }
            }

            m_storage->async_check_fastresume(
                &m_resume_entry,
                boost::bind(&transfer::on_resume_data_checked, shared_from_this(), _1, _2));
        }
        else
        {
            DBG("don't read resume data: {hash: " << hash() << ", file: " << name() << "}");
            set_state(transfer_status::seeding);
        }
    }

    void transfer::on_resume_data_checked(int ret, disk_io_job const& j)
    {
        boost::mutex::scoped_lock l(m_ses.m_mutex);

        if (ret == piece_manager::fatal_disk_error)
        {
            handle_disk_error(j);
            set_state(transfer_status::queued_for_checking);
            std::vector<char>().swap(m_resume_data);
            lazy_entry().swap(m_resume_entry);
            return;
        }

        // only report this error if the user actually provided resume data
        if ((j.error || ret != 0) && !m_resume_data.empty())
        {
            m_ses.m_alerts.post_alert_should(fastresume_rejected_alert(handle(), j.error));
            ERR("fastresume data rejected: " << j.error.message() << " ret: " << ret);
        }

        // if ret != 0, it means we need a full check. We don't necessarily need
        // that when the resume data check fails.
        if (ret == 0)
        {
            DBG("resume data check succeed: {hash: " << hash() << ", file: " << name() << "}");
            // pieces count will calculate by length pieces string
            int num_pieces = 0;

            if (!j.error && m_resume_entry.type() == lazy_entry::dict_t)
            {
                lazy_entry const* hv = m_resume_entry.dict_find_list("hashset-values");
                lazy_entry const* pieces = m_resume_entry.dict_find("pieces");

                std::vector<md4_hash> piece_hashses;
                piece_hashses.resize(hv->list_size());
                for (int n = 0; n < hv->list_size(); ++n)
                {
                    piece_hashses[n] = md4_hash::fromString(hv->list_at(n)->string_value());
                }
                m_info->piece_hashses(piece_hashses);

                // we don't compare pieces count
                if (pieces && pieces->type() == lazy_entry::string_t)
                {
                    num_pieces = pieces->string_length();
                    char const* pieces_str = pieces->string_ptr();
                    for (int i = 0, end(pieces->string_length()); i < end; ++i)
                    {
                        if (pieces_str[i] & 1) m_picker->we_have(i);
                    }
                }

                // parse unfinished pieces
                const int num_blocks_per_piece = div_ceil(PIECE_SIZE, BLOCK_SIZE);

                if (lazy_entry const* unfinished_ent = m_resume_entry.dict_find_list("unfinished"))
                {
                    for (int i = 0; i < unfinished_ent->list_size(); ++i)
                    {
                        lazy_entry const* e = unfinished_ent->list_at(i);
                        if (e->type() != lazy_entry::dict_t) continue;
                        int piece = e->dict_find_int_value("piece", -1);
                        if (piece < 0 || piece > num_pieces) continue;

                        if (m_picker->have_piece(piece))
                            m_picker->we_dont_have(piece);

                        std::string bitmask = e->dict_find_string_value("bitmask");
                        if (bitmask.empty()) continue;

                        const int num_bitmask_bytes = div_ceil(num_blocks_per_piece, 8);
                        if ((int)bitmask.size() != num_bitmask_bytes) continue;
                        for (int j = 0; j < num_bitmask_bytes; ++j)
                        {
                            unsigned char bits = bitmask[j];
                            int num_bits = (std::min)(num_blocks_per_piece - j*8, 8);
                            for (int k = 0; k < num_bits; ++k)
                            {
                                const int bit = j * 8 + k;
                                if (bits & (1 << k))
                                {
                                    m_picker->mark_as_finished(piece_block(piece, bit), 0);

                                    if (piece < int(piece_hashses.size()))
                                    {
                                        const md4_hash& hs = piece_hashses.at(piece);

                                        // check piece when we have hash for it
                                        if (m_picker->is_piece_finished(piece))
                                            async_verify_piece(piece, hs, boost::bind(&transfer::piece_finished
                                                , shared_from_this(), piece, _1));
                                    }
                                }
                            }
                        }
                    }
                }
            }

            file_checked();
        }
        else if (m_info->is_valid())
        {
            DBG("resume data check fails: {hash: " << hash() << ", file: " << name() << "}, metadata: valid");
            set_state(transfer_status::queued_for_checking);
            if (should_check_file())
                queue_transfer_check();
        }
        else
        {
            // TODO: we might request the metadata for checking
            DBG("resume data check fails: {hash: " << hash() << ", file: " << name() <<
                ", metadata: invalid}");
            file_checked();
        }

        std::vector<char>().swap(m_resume_data);
        lazy_entry().swap(m_resume_entry);
    }

    void transfer::second_tick(stat& accumulator, int tick_interval_ms, const ptime& now)
    {
        if (m_minute_timer.expired(now) && m_connections.size() == 0)
            request_peers();

        // if we're in upload only mode and we're auto-managed
        // leave upload mode every 10 minutes hoping that the error
        // condition has been fixed
        if (m_upload_mode && m_auto_managed &&
            int(m_upload_mode_time) >= m_ses.settings().optimistic_disk_retry)
        {
            set_upload_mode(false);
        }

        if (is_paused())
        {
            // let the stats fade out to 0
            accumulator += m_stat;
            m_stat.second_tick(tick_interval_ms);
            return;
        }

        if (active()) m_last_active = 0;
        else m_last_active += div_ceil(tick_interval_ms, 1000);

        for (std::set<peer_connection*>::iterator i = m_connections.begin();
             i != m_connections.end();)
        {
            peer_connection* p = *i;
            ++i;
            m_stat += p->statistics();

            try
            {
                p->second_tick(tick_interval_ms);
            }
            catch (std::exception& e)
            {
                DBG("**ERROR**: " << e.what());
                p->disconnect(errors::no_error, 1);
            }
        }

        if (m_upload_mode) ++m_upload_mode_time;

        accumulator += m_stat;
        m_total_uploaded += m_stat.last_payload_uploaded();
        m_total_downloaded += m_stat.last_payload_downloaded();
        m_stat.second_tick(tick_interval_ms);
    }

    void transfer::async_verify_piece(
        int piece_index, const md4_hash& hash, const boost::function<void(int)>& f)
    {
        LIBED2K_ASSERT(m_storage);
        LIBED2K_ASSERT(m_storage->refcount() > 0);
        LIBED2K_ASSERT(piece_index >= 0);
        LIBED2K_ASSERT(piece_index < m_info->num_pieces());
        LIBED2K_ASSERT(piece_index < (int)m_picker->num_pieces());
        LIBED2K_ASSERT(!m_picker || !m_picker->have_piece(piece_index));
#ifdef LIBED2K_DEBUG
        if (m_picker)
        {
            int blocks_in_piece = m_picker->blocks_in_piece(piece_index);
            for (int i = 0; i < blocks_in_piece; ++i)
            {
                LIBED2K_ASSERT(m_picker->num_peers(piece_block(piece_index, i)) == 0);
            }
        }
#endif
        m_storage->async_hash(
            piece_index, boost::bind(&transfer::on_piece_verified, shared_from_this(), _1, _2, f));
    }

    void transfer::on_piece_verified(int ret, disk_io_job const& j, boost::function<void(int)> f)
    {
        //LIBED2K_ASSERT(m_ses.is_network_thread());
        boost::mutex::scoped_lock l(m_ses.m_mutex);

        // return value:
        // 0: success, piece passed hash check
        // -1: disk failure
        // -2: hash check failed

        state_updated();

        if (ret == -1) handle_disk_error(j);
        f(ret);
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
        if (!m_picker->is_piece_finished(index))
        {
            ERR("piece was checked but have failed being written: "
                "{transfer: " << hash() << ", piece: " << index << "}");
            return;
        }

        if (passed_hash_check == 0)
        {
            // the following call may cause picker to become invalid
            // in case we just became a seed
            DBG("piece passed hash check: "
                "{transfer: " << hash() << ", piece: " << index << "}");
            piece_passed(index);
        }
        else if (passed_hash_check == -2)
        {
            DBG("piece failed hash check: "
                "{transfer: " << hash() << ", piece: " << index << "}");
            // piece_failed() will restore the piece
            piece_failed(index);
        }
        else
        {
            ERR("piece check failed with unexpected error: "
                "{transfer: " << hash() << ", piece: " << index <<
                ", error: " << passed_hash_check << "}");
            m_picker->restore_piece(index);
            restore_piece_state(index);
        }
    }

    shared_file_entry transfer::get_announce() const
    {
        shared_file_entry entry;

        // do not announce transfer without pieces or in checking state
        if (m_state == transfer_status::queued_for_checking
                || m_state == transfer_status::checking_files
                || m_state == transfer_status::checking_resume_data ||
                num_have() == 0)
        {
            return entry;
        }

        // TODO - implement generate file entry from transfer here
        entry.m_hFile = hash();
        if (m_ses.m_server_connection->tcp_flags() & SRV_TCPFLG_COMPRESSION)
        {
            if (!is_seed())
            {
                // publishing an incomplete file
                entry.m_network_point.m_nIP     = 0xFCFCFCFC;
                entry.m_network_point.m_nPort   = 0xFCFC;
            }
            else
            {
                // publishing a complete file
                entry.m_network_point.m_nIP     = 0xFBFBFBFB;
                entry.m_network_point.m_nPort   = 0xFBFB;
            }
        }
        else
        {
            entry.m_network_point.m_nIP     = m_ses.m_server_connection->client_id();
            entry.m_network_point.m_nPort   = m_ses.settings().listen_port;
        }

        entry.m_list.add_tag(make_string_tag(name(), FT_FILENAME, true));

        __file_size fs;
        fs.nQuadPart = size();
        entry.m_list.add_tag(make_typed_tag(fs.nLowPart, FT_FILESIZE, true));

        if (fs.nHighPart > 0)
        {
            entry.m_list.add_tag(make_typed_tag(fs.nHighPart, FT_FILESIZE_HI, true));
        }

        bool bFileTypeAdded = false;

        if (m_ses.m_server_connection->tcp_flags() & SRV_TCPFLG_TYPETAGINTEGER)
        {
            // Send integer file type tags to newer servers
            boost::uint32_t eFileType = GetED2KFileTypeSearchID(GetED2KFileTypeID(name()));

            if (eFileType >= ED2KFT_AUDIO && eFileType <= ED2KFT_EMULECOLLECTION)
            {
                entry.m_list.add_tag(make_typed_tag(eFileType, FT_FILETYPE, true));
                bFileTypeAdded = true;
            }
        }

        if (!bFileTypeAdded)
        {
            // Send string file type tags to:
            //  - newer servers, in case there is no integer type available for the file type (e.g. emulecollection)
            //  - older servers
            //  - all clients
            std::string strED2KFileType(GetED2KFileTypeSearchTerm(GetED2KFileTypeID(name())));

            if (!strED2KFileType.empty())
            {
                entry.m_list.add_tag(make_string_tag(strED2KFileType, FT_FILETYPE, true));
            }
        }

        return entry;
    }

    void transfer::save_resume_data(int flags)
    {
        if (!m_owning_storage.get())
        {
            m_ses.m_alerts.post_alert_should(
                save_resume_data_failed_alert(handle(), errors::destructing_transfer));
            return;
        }

        m_need_save_resume_data = false;

        LIBED2K_ASSERT(m_storage);
        if (m_state == transfer_status::queued_for_checking
            || m_state == transfer_status::checking_files
            || m_state == transfer_status::checking_resume_data)
        {
            boost::shared_ptr<entry> rd(new entry);
            write_resume_data(*rd);
            m_ses.m_alerts.post_alert_should(save_resume_data_alert(rd, handle()));
            return;
        }

        if (flags & transfer_handle::flush_disk_cache)
            m_storage->async_release_files();

        m_storage->async_save_resume_data(
            boost::bind(&transfer::on_save_resume_data, shared_from_this(), _1, _2));
    }

    void transfer::on_save_resume_data(int ret, disk_io_job const& j)
    {
        boost::mutex::scoped_lock l(m_ses.m_mutex);

        if (!j.resume_data)
        {
            m_ses.m_alerts.post_alert_should(save_resume_data_failed_alert(handle(), j.error));
        }
        else
        {
            m_need_save_resume_data = false;
            write_resume_data(*j.resume_data);
            m_ses.m_alerts.post_alert_should(save_resume_data_alert(j.resume_data, handle()));
        }
    }

    bool transfer::should_check_file() const
    {
        return
            (m_state == transfer_status::checking_files ||
             m_state == transfer_status::queued_for_checking)
            && !m_paused
            && !has_error()
            && !m_abort
            && !m_ses.is_paused();
    }

    void transfer::file_checked()
    {
        if (m_abort) return;

        DBG("file checked: {hash: " << hash() << ", file: " << name() << "}");

        // we might be finished already, in which case we should
        // not switch to downloading mode. If all files are
        // filtered, we're finished when we start.
        if (m_state != transfer_status::finished)
            set_state(transfer_status::downloading);

        m_ses.m_alerts.post_alert_should(transfer_checked_alert(handle()));

        if (!is_seed())
        {
            // if we just finished checking and we're not a seed, we are
            // likely to be unpaused
            // TODO - add automanage mode
            //if (m_ses.m_auto_manage_time_scaler > 1)
            //    m_ses.m_auto_manage_time_scaler = 1;

            if (is_finished() && m_state != transfer_status::finished)
                finished();
        }
        else
        {
            if (m_state != transfer_status::finished)
                finished();
        }

        // TODO
        // call on file checked callback
        // initialize connections

        //m_files_checked = true;
        //start_announcing();
    }

    void transfer::start_checking()
    {
        DBG("start checking: {hash: " << hash() << ", file: " << name() << "}");
        LIBED2K_ASSERT(should_check_file());
        set_state(transfer_status::checking_files);

        m_storage->async_check_files(
            boost::bind(&transfer::on_piece_checked, shared_from_this(), _1, _2));
    }

    void transfer::on_piece_checked(int ret, disk_io_job const& j)
    {
        boost::mutex::scoped_lock l(m_ses.m_mutex);

        if (ret == piece_manager::disk_check_aborted)
        {
            dequeue_transfer_check();
            pause();
            return;
        }
        if (ret == piece_manager::fatal_disk_error)
        {
            ERR("fatal disk error: {hash: " << hash() << ", file: " << name() <<
                ", error: " << j.error.message() << "}");
            m_ses.m_alerts.post_alert_should(file_error_alert(j.error_file, handle(), j.error));

            pause();
            set_error(j.error);
            return;
        }

        // TODO - should i use size_type?
        m_progress_ppm = size_type(j.piece) * 1000000 / num_pieces();

        LIBED2K_ASSERT(m_picker);
        if (j.offset >= 0 && !m_picker->have_piece(j.offset))
        {
            we_have(j.offset);
            //remove_time_critical_piece(j.offset);
        }

        // we're not done checking yet
        // this handler will be called repeatedly until
        // we're done, or encounter a failure
        if (ret == piece_manager::need_full_check) return;

        dequeue_transfer_check();
        file_checked();
    }

    void transfer::queue_transfer_check()
    {
        if (m_queued_for_checking) return;
        DBG("queue transfer check: {hash: " << hash() << ", file: " << name() << "}");
        m_queued_for_checking = true;
        m_ses.queue_check_transfer(shared_from_this());
    }

    void transfer::dequeue_transfer_check()
    {
        if (!m_queued_for_checking) return;
        DBG("dequeue transfer check: {hash: " << hash() << ", file: " << name() << "}");
        m_queued_for_checking = false;
        m_ses.dequeue_check_transfer(shared_from_this());
    }

    void transfer::set_error(error_code const& ec)
    {
        bool checking_file = should_check_file();
        m_error = ec;

        // set error and check checking status again
        if (checking_file && !should_check_file())
        {
            // stop checking and remove transfer from checking queue
            m_storage->abort_disk_io();
            dequeue_transfer_check();
            // transfer was set queue for checking because after set_error call
            // we always set pause, on_tick in session_impl doesn't check transfers on pause
            set_state(transfer_status::queued_for_checking);
        }
    }

    void transfer::write_resume_data(entry& ret) const
    {
        ret["file-format"] = "libed2k resume file";
        ret["file-version"] = 1;
        ret["libed2k-version"] = LIBED2K_VERSION;

        ret["total_uploaded"] = m_total_uploaded;
        ret["total_downloaded"] = m_total_downloaded;

        ret["num_seeds"] = m_complete;
        ret["num_downloaders"] = m_incomplete;

        ret["sequential_download"] = m_sequential_download;

        ret["transfer-hash"] = hash().toString();

        // blocks per piece
        const int num_blocks_per_piece = div_ceil(PIECE_SIZE, BLOCK_SIZE);

        // if this transfer is a seed, we won't have a piece picker
        // and there will be no half-finished pieces.
        if (!is_seed())
        {
            const std::vector<piece_picker::downloading_piece>& q
                = m_picker->get_download_queue();

            // unfinished pieces
            ret["unfinished"] = entry::list_type();
            entry::list_type& up = ret["unfinished"].list();

            // info for each unfinished piece
            for (std::vector<piece_picker::downloading_piece>::const_iterator i
                = q.begin(); i != q.end(); ++i)
            {
                if (i->finished == 0) continue;

                entry piece_struct(entry::dictionary_t);

                // the unfinished piece's index
                piece_struct["piece"] = i->index;

                std::string bitmask;
                const int num_bitmask_bytes = div_ceil(num_blocks_per_piece, 8);

                for (int j = 0; j < num_bitmask_bytes; ++j)
                {
                    unsigned char v = 0;
                    int bits = (std::min)(num_blocks_per_piece - j*8, 8);
                    for (int k = 0; k < bits; ++k)
                        v |= (i->info[j*8+k].state == piece_picker::block_info::state_finished)
                        ? (1 << k) : 0;
                    bitmask.append(1, v);
                    LIBED2K_ASSERT(bits == 8 || j == num_bitmask_bytes - 1);
                }
                piece_struct["bitmask"] = bitmask;
                // push the struct onto the unfinished-piece list
                up.push_back(piece_struct);
            }
        }

        // write have bitmask
        // the pieces string has one byte per piece. Each
        // byte is a bitmask representing different properties
        // for the piece
        // bit 0: set if we have the piece
        // bit 1: set if we have verified the piece (in seed mode)
        entry::string_type& pieces = ret["pieces"].string();
        pieces.resize(m_info->num_pieces());

        if (is_seed())
        {
            std::memset(&pieces[0], 1, pieces.size());
        }
        else
        {
            for (int i = 0, end(pieces.size()); i < end; ++i)
                pieces[i] = m_picker->have_piece(i) ? 1 : 0;
        }

        // store current hashset
        ret["hashset-values"]  = entry::list_type();
        entry::list_type& hv = ret["hashset-values"].list();

        const std::vector<md4_hash>& piece_hashses = m_info->piece_hashses();
        for (size_t n = 0; n < piece_hashses.size(); ++n)
        {
            hv.push_back(piece_hashses.at(n).toString());
        }

        ret["upload_rate_limit"] = upload_limit();
        ret["download_rate_limit"] = download_limit();
        // TODO - add real values
        ret["max_connections"] = 1000; //max_connections();
        ret["max_uploads"] = 1000; //max_uploads();
        ret["paused"] = m_paused;
        ret["auto_managed"] = true;

        // write piece priorities
        entry::string_type& piece_priority = ret["piece_priority"].string();
        piece_priority.resize(m_info->num_pieces());

        if (is_seed())
        {
            std::memset(&piece_priority[0], 1, pieces.size());
        }
        else
        {
            for (int i = 0, end(piece_priority.size()); i < end; ++i)
                piece_priority[i] = m_picker->piece_priority(i);
        }
    }

    void transfer::read_resume_data(lazy_entry const& rd)
    {
        m_total_uploaded = rd.dict_find_int_value("total_uploaded");
        m_total_downloaded = rd.dict_find_int_value("total_downloaded");
        set_upload_limit(rd.dict_find_int_value("upload_rate_limit", -1));
        set_download_limit(rd.dict_find_int_value("download_rate_limit", -1));
        m_complete = rd.dict_find_int_value("num_seeds", -1);
        m_incomplete = rd.dict_find_int_value("num_downloaders", -1);

        int sequential_ = rd.dict_find_int_value("sequential_download", -1);
        if (sequential_ != -1) set_sequential_download(sequential_);

        int paused_ = rd.dict_find_int_value("paused", -1);
        if (paused_ != -1) m_paused = paused_;
    }

    void transfer::handle_disk_write(const disk_io_job& j, peer_connection* c)
    {
        if (is_seed()) return;

        LIBED2K_ASSERT(j.piece >= 0);
        LIBED2K_ASSERT(m_picker);

        piece_block block_finished(j.piece, j.offset/BLOCK_SIZE);
        m_picker->mark_as_finished(block_finished, c->get_peer());
        m_need_save_resume_data = true;
    }

    void transfer::handle_disk_error(disk_io_job const& j, peer_connection* c)
    {
        if (!j.error) return;

        ERR("disk error: '" << j.error.message() << "' in file " << j.error_file);

        LIBED2K_ASSERT(j.piece >= 0);

        piece_block block(j.piece, j.offset/BLOCK_SIZE);

        if (j.action == disk_io_job::write)
        {
            ERR("block write failed: {piece: " <<  block.piece_index <<
                ", block: " << block.block_index << " }");
            // we failed to write j.piece to disk tell the piece picker
            if (has_picker() && j.piece >= 0) picker().write_failed(block);
        }

        if (j.error ==
#if BOOST_VERSION == 103500
            error_code(boost::system::posix_error::not_enough_memory, get_posix_category())
#elif BOOST_VERSION > 103500
            error_code(boost::system::errc::not_enough_memory, get_posix_category())
#else
            asio::error::no_memory
#endif
            )
        {
            m_ses.m_alerts.post_alert_should(file_error_alert(j.error_file, handle(), j.error));

            if (c) c->disconnect(errors::no_memory);
            return;
        }

        m_ses.m_alerts.post_alert_should(file_error_alert(j.error_file, handle(), j.error));

        if (j.action == disk_io_job::write)
        {
            // if we failed to write, stop downloading and just
            // keep seeding.
            // TODO: make this depend on the error and on the filesystem the
            // files are being downloaded to. If the error is no_space_left_on_device
            // and the filesystem doesn't support sparse files, only zero the priorities
            // of the pieces that are at the tails of all files, leaving everything
            // up to the highest written piece in each file
            set_upload_mode(true);
            return;
        }

        // put the transfer in an error-state
        set_error(j.error);
        pause();
    }

    bool transfer::active() const
    {
        return m_connections.size() > 0 || !is_seed();
    }

    void transfer::activate(bool act)
    {
        if (act && active() && m_ses.add_active_transfer(shared_from_this()))
            m_last_active = 0;
        else if (!act && !active()) m_ses.remove_active_transfer(shared_from_this());
    }

    shared_file_entry transfer2sfe(const std::pair<md4_hash, boost::shared_ptr<transfer> >& tran)
    {
        return tran.second->get_announce();
    }

}
