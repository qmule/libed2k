#include <libed2k/version.hpp>
#include <libed2k/session.hpp>
#include <libed2k/session_impl.hpp>
#include <libed2k/transfer.hpp>
#include <libed2k/peer.hpp>
#include <libed2k/peer_connection.hpp>
#include <libed2k/server_connection.hpp>
#include <libed2k/constants.hpp>
#include <libed2k/util.hpp>
#include <libed2k/file.hpp>
#include <libed2k/alert_types.hpp>

namespace libed2k
{

    /**
      * fake constructor
     */
    transfer::transfer(aux::session_impl& ses, const std::vector<peer_entry>& pl,
                       const md4_hash& hash, const fs::path p, fsize_t size):
        m_ses(ses),
        m_filehash(hash),
        m_filepath(p),
        m_filesize(size),
        m_complete(-1),
        m_incomplete(-1),
        m_policy(this, pl),
        m_minute_timer(time::minutes(1), time::min_date_time)
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
        m_filehash(p.file_hash),
        m_filepath(p.file_path),
        m_collectionpath(p.collection_path),
        m_filesize(p.file_size),
        m_verified(p.pieces),
        m_hashset(p.hashset),
        m_storage_mode(p.storage_mode),
        m_state(transfer_status::checking_resume_data),
        m_seed_mode(p.seed_mode),
        m_complete(p.num_complete_sources),
        m_incomplete(p.num_incomplete_sources),
        m_policy(this, p.peer_list),
        m_info(new transfer_info(libtorrent::sha1_hash())),
        m_accepted(p.accepted),
        m_requested(p.requested),
        m_transferred(p.transferred),
        m_priority(p.priority),
        m_total_uploaded(0),
        m_total_downloaded(0),
        m_queued_for_checking(false),
        m_progress_ppm(0),
        m_minute_timer(time::minutes(1), time::min_date_time)
    {
        DBG("transfer file size: " << m_filesize);
        if (m_verified.size() == 0)
            m_verified.resize(piece_count(m_filesize), 0);

        if (p.resume_data) m_resume_data.swap(*p.resume_data);

        assert(m_verified.size() == num_pieces());
    }

    transfer::~transfer()
    {
        if (!m_connections.empty())
            disconnect_all(errors::transfer_aborted);
    }

    transfer_handle transfer::handle()
    {
        return transfer_handle(shared_from_this());
    }

    void transfer::start()
    {
        if (!m_seed_mode)
        {
            DBG("transfer::start: prepare picker");
            m_picker.reset(new libtorrent::piece_picker());
            // TODO: file progress

            if (!m_resume_data.empty())
            {
                if (libtorrent::lazy_bdecode(&m_resume_data[0], &m_resume_data[0]
                    + m_resume_data.size(), m_resume_entry) != 0)
                {
                    ERR("fast resume parse error");
                    std::vector<char>().swap(m_resume_data);
                    m_ses.m_alerts.post_alert_should(fastresume_rejected_alert(handle(), errors::fast_resume_parse_error));
                }
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
        // files belonging to the torrents
        disconnect_all(errors::transfer_aborted);

        // post a message to the main thread to destruct
        // the torrent object from there
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
    }

    void transfer::update_collection(const fs::path pc)
    {
        m_collectionpath = pc;
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

    bool transfer::want_more_connections() const
    {
        return !m_abort && !is_paused() && !is_seed();
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

        try
        {
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

    bool transfer::attach_peer(peer_connection* p)
    {
        assert(!p->has_transfer());

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
        if (!m_policy.new_connection(*p))
            return false;

        assert(m_connections.find(p) == m_connections.end());
        m_connections.insert(p);
        return true;
    }

    void transfer::remove_peer(peer_connection* c)
    {
        std::set<peer_connection*>::iterator i = m_connections.find(c);
        if (i == m_connections.end())
        {
            assert(false);
            return;
        }

        if (ready_for_connections())
        {
            assert(c->get_transfer().lock().get() == this);

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

        m_policy.connection_closed(*c);
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

    bool transfer::try_connect_peer()
    {
        return m_policy.connect_one_peer();
    }

    void transfer::piece_passed(int index)
    {
        bool was_finished = (num_have() == num_pieces());
        we_have(index);
        verified(index);

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

    size_t transfer::num_pieces() const
    {
        return static_cast<size_t>(div_ceil(m_filesize, PIECE_SIZE));
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

    // called when torrent is complete (all pieces downloaded)
    void transfer::completed()
    {
        m_picker.reset();

        set_state(transfer_status::seeding);
        //if (!m_announcing) return;

        //announce_with_tracker();
    }

    // called when torrent is finished (all interesting
    // pieces have been downloaded)
    void transfer::finished()
    {
        DBG("file transfer '" << m_filepath.filename() << "' completed");
        //TODO: post alert

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

    void transfer::pause()
    {
        if (m_paused) return;
        m_paused = true;

        // TODO - why so nothing when session paused?
        //if (m_ses.is_paused()) return;

        DBG("pause transfer {hash: " << hash() << "}");

        // this will make the storage close all
        // files and flush all cached data
        if (m_owning_storage.get())
        {
            assert(m_storage);
            m_storage->async_release_files(
                boost::bind(&transfer::on_transfer_paused, shared_from_this(), _1, _2));
            m_storage->async_clear_read_cache();
        }
        else
        {
            m_ses.m_alerts.post_alert_should(paused_transfer_alert(handle()));
        }

        disconnect_all(errors::transfer_paused);
    }

    void transfer::resume()
    {
        if (!m_paused) return;
        DBG("resume transfer {hash: " << hash() << "}");
        m_paused = false;
        m_ses.m_alerts.post_alert_should(resumed_transfer_alert(handle()));
    }

    void transfer::set_upload_limit(int limit)
    {

    }

    int transfer::upload_limit() const
    {
        return 0;
    }

    void transfer::set_download_limit(int limit)
    {

    }

    int transfer::download_limit() const
    {
        return 0;
    }

    storage_interface* transfer::get_storage()
    {
        if (!m_owning_storage) return 0;
        return m_owning_storage->get_storage_impl();
    }

    void transfer::move_storage(const fs::path& save_path)
    {
        if (m_owning_storage.get())
        {
            m_owning_storage->async_move_storage(
                save_path.string(), boost::bind(&transfer::on_storage_moved, shared_from_this(), _1, _2));
        }
        else
        {
            m_ses.m_alerts.post_alert_should(storage_moved_alert(handle(), save_path.string()));
            m_filepath = save_path / m_filepath.filename();
        }
    }

    bool transfer::rename_file(const std::string& name)
    {
        DBG("renaming file in transfer {hash: " << m_filehash <<
            ", from: " << m_filepath.filename() << ", to: " << name << "}");
        if (!m_owning_storage.get()) return false;

        m_owning_storage->async_rename_file(
            0, name, boost::bind(&transfer::on_file_renamed, shared_from_this(), _1, _2));
        return true;
    }

    void transfer::delete_files()
    {
        DBG("deleting file in transfer {hash: " << m_filehash << ", files: " << m_filepath << "}");

        disconnect_all(errors::transfer_removed);

        if (m_owning_storage.get())
        {
            assert(m_storage);
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
        assert(m_picker.get());
        assert(index >= 0);
        assert(index < int(num_pieces()));
        if (index < 0 || index >= int(num_pieces())) return;

        m_picker->set_piece_priority(index, priority);
    }

    int transfer::piece_priority(int index) const
    {
        if (is_seed()) return 1;

        // this call is only valid on transfers with metadata
        assert(m_picker.get());
        assert(index >= 0);
        assert(index < int(num_pieces()));
        if (index < 0 || index >= int(num_pieces())) return 0;

        return m_picker->piece_priority(index);
    }

    void transfer::set_sequential_download(bool sd) { m_sequential_download = sd; }

    void transfer::piece_failed(int index)
    {
    }

    void transfer::restore_piece_state(int index)
    {
    }

    int transfer::num_seeds() const
    {
        int ret = 0;
        for (std::set<peer_connection*>::const_iterator i = m_connections.begin(),
                 end(m_connections.end()); i != end; ++i)
            if ((*i)->is_seed()) ++ret;
        return ret;
    }

    bool transfer::is_paused() const
    {
        return m_paused || m_ses.is_paused();
    }

    transfer_status transfer::status() const
    {
        transfer_status st;

        st.seed_mode = m_seed_mode;
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

        // transfer rate
        st.download_rate = m_stat.download_rate();
        st.upload_rate = m_stat.upload_rate();
        st.download_payload_rate = m_stat.download_payload_rate();
        st.upload_payload_rate = m_stat.upload_payload_rate();

        st.num_peers = (int)std::count_if(
            m_connections.begin(), m_connections.end(),
            !boost::bind(&peer_connection::is_connecting, _1));

        st.list_peers = m_policy.num_peers();
        //st.list_seeds = m_policy.num_seeds();
        //st.connect_candidates = m_policy.num_connect_candidates();
        st.num_connections = m_connections.size();
        st.connections_limit = 0; // TODO: m_max_connections;

        st.state = m_state;

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

    // fills in total_wanted, total_wanted_done and total_done
    void transfer::bytes_done(transfer_status& st) const
    {
        st.total_wanted = filesize();
        st.total_done = std::min<fsize_t>(num_have() * PIECE_SIZE, st.total_wanted);
        st.total_wanted_done = st.total_done;

        if (!m_picker) return;

        const int last_piece = num_pieces() - 1;

        // if we have the last piece, we have to correct
        // the amount we have, since the first calculation
        // assumed all pieces were of equal size
        if (m_picker->have_piece(last_piece))
        {
            int corr = filesize() % PIECE_SIZE - PIECE_SIZE;
            assert(corr <= 0);
            assert(corr > -int(PIECE_SIZE));
            st.total_done += corr;
            if (m_picker->piece_priority(last_piece) != 0)
            {
                assert(st.total_wanted_done >= int(PIECE_SIZE));
                st.total_wanted_done += corr;
            }
        }
        assert(st.total_wanted >= st.total_wanted_done);

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
            assert(i->finished <= m_picker->blocks_in_piece(index));

            for (int j = 0; j < blocks_per_piece; ++j)
            {
                if (i->info[j].state == piece_picker::block_info::state_writing ||
                    i->info[j].state == piece_picker::block_info::state_finished)
                {
                    corr += block_bytes_wanted(piece_block(index, j));
                }
                assert(corr >= 0);
                //assert(index != last_piece || j < m_picker->blocks_in_last_piece() ||
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

        assert(j.piece == 0);

        if (ret == 0)
        {
            DBG("file successfully renamed {hash: " << m_filehash << ", to: " << j.str << "}");
            m_ses.m_alerts.post_alert_should(file_renamed_alert(handle(), j.str));
            m_filepath.remove_filename() /= j.str;
        }
        else
        {
            DBG("file rename failed {hash: " << m_filehash << ", err: " << j.error << "}");
            m_ses.m_alerts.post_alert_should(
                file_rename_failed_alert(handle(), j.error));
        }
    }

    void transfer::on_storage_moved(int ret, disk_io_job const& j)
    {
        boost::mutex::scoped_lock l(m_ses.m_mutex);

        if (ret == 0)
        {
            DBG("storage successfully moved {hash: " << m_filehash << ", to: " << j.str << "}");
            m_ses.m_alerts.post_alert_should(storage_moved_alert(handle(), j.str));
            m_filepath = fs::path(j.str) / m_filepath.filename();
        }
        else
        {
            DBG("storage move failed {hash: " << m_filehash << ", err: " << j.error << "}");
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

    void transfer::on_disk_error(disk_io_job const& j, peer_connection* c)
    {
        if (!j.error) return;
        ERR("disk error: '" << j.error.message() << " in file " << j.error_file);
    }

    void transfer::init()
    {
        DBG("transfer::init: " << convert_to_native(m_filepath.filename()));
        file_storage& files = const_cast<file_storage&>(m_info->files());
        files.set_num_pieces(num_pieces());
        files.set_piece_length(PIECE_SIZE);
        files.add_file(m_filepath.filename(), m_filesize);

        // we have only one file with normal priority
        std::vector<boost::uint8_t> file_prio;
        file_prio.push_back(1);

        // the shared_from_this() will create an intentional
        // cycle of ownership, see the hpp file for description.
        m_owning_storage = new piece_manager(
            shared_from_this(), m_info, m_filepath.parent_path().string(), m_ses.m_filepool,
            m_ses.m_disk_thread, default_storage_constructor, m_storage_mode, file_prio);
        m_storage = m_owning_storage.get();

        if (has_picker())
        {
            int blocks_per_piece = div_ceil(PIECE_SIZE, BLOCK_SIZE);
            int blocks_in_last_piece = div_ceil(m_filesize % PIECE_SIZE, BLOCK_SIZE);
            m_picker->init(blocks_per_piece, blocks_in_last_piece, num_pieces());
        }

        set_state(transfer_status::checking_resume_data);

        if (m_resume_entry.type() == lazy_entry::dict_t)
        {
            DBG("transfer::init() = read resume data");
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
                m_ses.m_alerts.post_alert_should(fastresume_rejected_alert(handle(), error_code(ev,  get_libed2k_category())));
            }
            else
            {
                read_resume_data(m_resume_entry);
            }
        }

        m_storage->async_check_fastresume(&m_resume_entry
                    , boost::bind(&transfer::on_resume_data_checked
                    , shared_from_this(), _1, _2));
    }

    void transfer::on_resume_data_checked(int ret, disk_io_job const& j)
    {
        DBG("transfer::on_resume_data_checked");
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
            // pieces count will calculate by length pieces string
            int num_pieces = 0;

            if (!j.error && m_resume_entry.type() == lazy_entry::dict_t)
            {
                lazy_entry const* hv = m_resume_entry.dict_find_list("hashset-values");
                lazy_entry const* pieces = m_resume_entry.dict_find("pieces");

                m_hashset.resize(hv->list_size());

                for (int n = 0; n < hv->list_size(); ++n)
                {
                    m_hashset[n] = md4_hash::fromString(hv->list_at(n)->string_value());
                }

                // we don't compare pieces count
                if (pieces && pieces->type() == lazy_entry::string_t)
                {
                    num_pieces = pieces->string_length();
                    char const* pieces_str = pieces->string_ptr();
                    for (int i = 0, end(pieces->string_length()); i < end; ++i)
                    {
                        if (pieces_str[i] & 1) m_picker->we_have(i);
                        if (m_seed_mode && (pieces_str[i] & 2)) m_verified.set_bit(i);
                    }
                }

                // parse unfinished pieces
                int num_blocks_per_piece = div_ceil(PIECE_SIZE, BLOCK_SIZE);

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

                        const int num_bitmask_bytes = (std::max)(num_blocks_per_piece / 8, 1);
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

                                    if (piece < int(m_hashset.size()))
                                    {
                                        const md4_hash& hs = m_hashset.at(piece);

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
        else
        {
            set_state(transfer_status::queued_for_checking);
            if (should_check_file())
            queue_transfer_check();
        }

        std::vector<char>().swap(m_resume_data);
        lazy_entry().swap(m_resume_entry);
    }

    void transfer::second_tick(stat& accumulator, int tick_interval_ms)
    {
        if (m_minute_timer.expires() && want_more_peers()) request_peers();

        if (is_paused())
        {
            // let the stats fade out to 0
            accumulator += m_stat;
            m_stat.second_tick(tick_interval_ms);
            return;
        }

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

        accumulator += m_stat;
        m_total_uploaded += m_stat.last_payload_uploaded();
        m_total_downloaded += m_stat.last_payload_downloaded();
        m_stat.second_tick(tick_interval_ms);
    }

    void transfer::async_verify_piece(
        int piece_index, const md4_hash& hash, const boost::function<void(int)>& fun)
    {
        //TODO: piece verification
        m_ses.m_io_service.post(boost::bind(fun, 0));
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

    shared_file_entry transfer::getAnnounce() const
    {
        shared_file_entry entry;

        // do not announce transfer without pieces or in checking state
        if (m_state == transfer_status::queued_for_checking
                || m_state == transfer_status::checking_files
                || m_state == transfer_status::checking_resume_data ||
                num_pieces() == 0)
        {
            return entry;
        }

        // TODO - implement generate file entry from transfer here
        entry.m_hFile = m_filehash;
        if (m_ses.m_server_connection->tcp_flags() & SRV_TCPFLG_COMPRESSION)
        {
            DBG("server support compression");

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
            DBG("servers isn't support compression");
            entry.m_network_point.m_nIP     = m_ses.m_server_connection->client_id();
            entry.m_network_point.m_nPort   = m_ses.settings().listen_port;
        }

        entry.m_list.add_tag(make_string_tag(m_filepath.filename(), FT_FILENAME, true));

        __file_size fs;
        fs.nQuadPart = m_filesize;
        entry.m_list.add_tag(make_typed_tag(fs.nLowPart, FT_FILESIZE, true));

        if (fs.nHighPart > 0)
        {
            entry.m_list.add_tag(make_typed_tag(fs.nHighPart, FT_FILESIZE_HI, true));
        }

        bool bFileTypeAdded = false;

        if (m_ses.m_server_connection->tcp_flags() & SRV_TCPFLG_TYPETAGINTEGER)
        {
            // Send integer file type tags to newer servers
            boost::uint32_t eFileType = GetED2KFileTypeSearchID(GetED2KFileTypeID(m_filepath.string()));

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
            std::string strED2KFileType(GetED2KFileTypeSearchTerm(GetED2KFileTypeID(m_filepath.string())));

            if (!strED2KFileType.empty())
            {
                entry.m_list.add_tag(make_string_tag(strED2KFileType, FT_FILETYPE, true));
            }
        }

        return entry;
    }

    void transfer::save_resume_data()
    {
        if (!m_owning_storage.get())
        {
            m_ses.m_alerts.post_alert_should(save_resume_data_failed_alert(handle()
                , errors::destructing_transfer));
            return;
        }

        BOOST_ASSERT(m_storage);

        if (m_state == transfer_status::queued_for_checking
            || m_state == transfer_status::checking_files
            || m_state == transfer_status::checking_resume_data)
        {
            boost::shared_ptr<entry> rd(new entry);
            write_resume_data(*rd);
            m_ses.m_alerts.post_alert_should(save_resume_data_alert(rd, handle()));
            return;
        }

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
            write_resume_data(*j.resume_data);
            m_ses.m_alerts.post_alert_should(save_resume_data_alert(j.resume_data, handle()));
        }
    }


    bool transfer::should_check_file() const
    {
        return (m_state == transfer_status::checking_files
            || m_state == transfer_status::queued_for_checking)
            //&& (!m_paused /*|| m_auto_managed*/)
            && !has_error()
            && !m_abort;
    }

    void transfer::file_checked()
    {
        DBG("transfer::file_checked");
        //BOOST_ASSERT(m_torrent_file->is_valid());

        if (m_abort) return;

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
        DBG("transfer::start_checking");
        BOOST_ASSERT(should_check_file());
        set_state(transfer_status::checking_files);

        dequeue_transfer_check();
        file_checked();
        // TODO - activate real checking!
        //m_storage->async_check_files(boost::bind(
        //            &transfer::on_piece_checked
        //            , shared_from_this(), _1, _2));
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
            if (m_ses.m_alerts.should_post<file_error_alert>())
            {
                m_ses.m_alerts.post_alert_should(file_error_alert(j.error_file, handle(), j.error));
            }

#if defined LIBED2K_VERBOSE_LOGGING || defined LIBED2K_LOGGING || defined LIBED2K_ERROR_LOGGING
            (*m_ses.m_logger) << time_now_string() << ": fatal disk error ["
                " error: " << j.error.message() <<
                " torrent: " << torrent_file().name() <<
                " ]\n";
#endif
            pause();
            set_error(j.error);
            return;
        }

        // TODO - should i use size_type?
        m_progress_ppm = fsize_t(j.piece) * PIECE_SIZE / num_pieces();

        LIBED2K_ASSERT(m_picker);
        if (j.offset >= 0 && !m_picker->have_piece(j.offset))
            we_have(j.offset);

        // what is it?
        //remove_time_critical_piece(j.offset);

        // we're not done checking yet
        // this handler will be called repeatedly until
        // we're done, or encounter a failure
        if (ret == piece_manager::need_full_check) return;

        dequeue_transfer_check();
        file_checked();
    }


    void transfer::queue_transfer_check()
    {
        DBG("transfer::queue_transfer_check");
        if (m_queued_for_checking) return;
        DBG("transfer::queue_transfer_check: start");
        m_queued_for_checking = true;
        m_ses.queue_check_torrent(shared_from_this());
    }

    void transfer::dequeue_transfer_check()
    {
        if (!m_queued_for_checking) return;
        m_queued_for_checking = false;
        m_ses.dequeue_check_torrent(shared_from_this());
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

        ret["seed_mode"] = m_seed_mode;

        ret["transfer-hash"] = hash().toString();

        // blocks per piece
        int num_blocks_per_piece = div_ceil(PIECE_SIZE, BLOCK_SIZE);

        // if this torrent is a seed, we won't have a piece picker
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
                const int num_bitmask_bytes
                    = (std::max)(num_blocks_per_piece / 8, 1);

                for (int j = 0; j < num_bitmask_bytes; ++j)
                {
                    unsigned char v = 0;
                    int bits = (std::min)(num_blocks_per_piece - j*8, 8);
                    for (int k = 0; k < bits; ++k)
                        v |= (i->info[j*8+k].state == piece_picker::block_info::state_finished)
                        ? (1 << k) : 0;
                    bitmask.insert(bitmask.end(), v);
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

        if (m_seed_mode)
        {
            BOOST_ASSERT(m_verified.size() == pieces.size());
            for (int i = 0, end(pieces.size()); i < end; ++i)
                pieces[i] |= m_verified[i] ? 2 : 0;
        }

        // store current hashset
        ret["hashset-values"]  = entry::list_type();
        entry::list_type& hv = ret["hashset-values"].list();

        for (size_t n = 0; n < m_hashset.size(); ++n)
        {
            hv.push_back(m_hashset.at(n).toString());
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
        m_seed_mode = rd.dict_find_int_value("seed_mode", 0);
        m_complete = rd.dict_find_int_value("num_seeds", -1);
        m_incomplete = rd.dict_find_int_value("num_downloaders", -1);

        int sequential_ = rd.dict_find_int_value("sequential_download", -1);
        if (sequential_ != -1) set_sequential_download(sequential_);

        int paused_ = rd.dict_find_int_value("paused", -1);
        if (paused_ != -1) m_paused = paused_;
    }

    void transfer::handle_disk_error(disk_io_job const& j, peer_connection* c)
    {
        if (!j.error) return;

        ERR("disk error: '" << j.error.message()
            << " in file " << j.error_file);

        LIBED2K_ASSERT(j.piece >= 0);

        piece_block block_finished(j.piece, div_ceil(j.offset, BLOCK_SIZE));

        if (j.action == disk_io_job::write)
        {
            // we failed to write j.piece to disk tell the piece picker
            if (has_picker() && j.piece >= 0) picker().write_failed(block_finished);
        }

        if (j.error == error_code(boost::system::errc::not_enough_memory, get_posix_category()))
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
            //set_upload_mode(true); //TODO ?
            return;
        }

        // put the torrent in an error-state
        set_error(j.error);
        pause();
    }

    std::string transfer2catalog(const std::pair<md4_hash, boost::shared_ptr<transfer> >& tran)
    {
        if (tran.second->collectionpath().empty())
        {
            return std::string("");
        }

        return tran.second->filepath().directory_string();
    }

    shared_file_entry transfer2sfe(const std::pair<md4_hash, boost::shared_ptr<transfer> >& tran)
    {
        return tran.second->getAnnounce();
    }

}
