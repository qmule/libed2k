
#ifndef __LIBED2K_TRANSFER__
#define __LIBED2K_TRANSFER__

#include <set>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/filesystem.hpp>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>

#include "libed2k/lazy_entry.hpp"
#include "libed2k/policy.hpp"
#include "libed2k/piece_picker.hpp"
#include "libed2k/packet_struct.hpp"
#include "libed2k/peer_info.hpp"
#include "libed2k/storage_defs.hpp"
#include "libed2k/entry.hpp"
#include "libed2k/stat.hpp"
#include "libed2k/transfer_handle.hpp"

namespace libed2k
{
    class transfer_info;
    class add_transfer_params;
    namespace aux{
        class session_impl;
    }
    class peer;
    class server_request;
    class server_response;
    class piece_manager;
    struct disk_io_job;

    // a transfer is a class that holds information
    // for a specific download. It updates itself against
    // the tracker
    class transfer : public boost::enable_shared_from_this<transfer>
    {
    public:
        /**
         * it is fake transfer constructor for using in unit tests
         * you shouldn't it anywhere except unit tests
         */
        transfer(aux::session_impl& ses, const std::vector<peer_entry>& pl,
                 const md4_hash& hash, const std::string& filename, size_type size);


        transfer(aux::session_impl& ses, tcp::endpoint const& net_interface,
                 int seq, add_transfer_params const& p);
        ~transfer();

        const md4_hash& hash() const;
        // TODO temp code - it will be part of new share files engine
        const md4_hash collection_hash() const;
        size_type size() const;
        const std::string& name() const;
        const std::string& save_path() const;


        const std::vector<md4_hash>& piece_hashses() const;
        void piece_hashses(const std::vector<md4_hash>& hs);
        const md4_hash& hash_for_piece(size_t piece) const;

        transfer_handle handle();
        void start();
        void abort();
        void set_state(transfer_status::state_t s);

        aux::session_impl& session() { return m_ses; }

        bool want_more_peers() const;
        void request_peers();
        void add_peer(const tcp::endpoint& peer);
        bool connect_to_peer(peer* peerinfo);
        // used by peer_connection to attach itself to a torrent
        // since incoming connections don't know what torrent
        // they're a part of until they have received an info_hash.
        // false means attach failed
        bool attach_peer(peer_connection* peer);
        // this will remove the peer and make sure all
        // the pieces it had have their reference counter
        // decreased in the piece_picker
        void remove_peer(peer_connection* p);
        void get_peer_info(std::vector<peer_info>& infos);
        bool has_peer(peer_connection* p) const
        { return m_connections.find(p) != m_connections.end(); }

        bool want_more_connections() const;
        void disconnect_all(const error_code& ec);
        bool try_connect_peer();
        void give_connect_points(int points);
        bool has_error() const { return m_error; }

        // the number of peers that belong to this torrent
        int num_peers() const { return (int)m_connections.size(); }
        int num_seeds() const;

        bool is_paused() const;
        bool is_seed() const
        {
            return !m_picker || m_picker->num_have() == m_picker->num_pieces();
        }

        // this is true if we have all the pieces that we want
        bool is_finished() const
        {
            if (is_seed()) return true;
            return (num_pieces() - m_picker->num_have() == 0);
        }

        bool is_aborted() const { return m_abort; }
        bool is_announced() const { return m_announced; }
        void set_announced(bool announced) { m_announced = announced; }
        transfer_status::state_t state() const { return m_state; }
        transfer_status status() const;

        void pause();
        void resume();
        void set_upload_limit(int limit);
        int upload_limit() const;
        void set_download_limit(int limit);
        int download_limit() const;

        void piece_availability(std::vector<int>& avail) const;

        void set_piece_priority(int index, int priority);
        int piece_priority(int index) const;

        void set_sequential_download(bool sd);
        bool is_sequential_download() const { return m_sequential_download; }

        int queue_position() const { return m_sequence_number; }

        void second_tick(stat& accumulator, int tick_interval_ms);

        // this is called wheh the transfer has completed
        // the download. It will post an event, disconnect
        // all seeds and let the tracker know we're finished.
        void completed();

        // this is called when the transfer has finished. i.e.
        // all the pieces we have not filtered have been downloaded.
        // If no pieces are filtered, this is called first and then
        // completed() is called immediately after it.
        void finished();

        stat statistics() const { return m_stat; }
        void add_stats(const stat& s);

        void set_upload_mode(bool b);
        bool upload_mode() const { return m_upload_mode; }

        // --------------------------------------------
        // PIECE MANAGEMENT
        // --------------------------------------------
        void async_verify_piece(int piece_index, const md4_hash& hash,
                                const boost::function<void(int)>& f);

        // this is called from the peer_connection
        // each time a piece has failed the hash test
        void piece_finished(int index, int passed_hash_check);

        // piece_passed is called when a piece passes the hash check
        // this will tell all peers that we just got his piece
        // and also let the piece picker know that we have this piece
        // so it wont pick it for download
        void piece_passed(int index);

        // piece_failed is called when a piece fails the hash check
        void piece_failed(int index);

        // this will restore the piece picker state for a piece
        // by re marking all the requests to blocks in this piece
        // that are still outstanding in peers' download queues.
        // this is done when a piece fails
        void restore_piece_state(int index);

        bool has_picker() const
        {
            return m_picker.get() != 0;
        }
        piece_picker& picker() { return *m_picker; }

        // returns true if we have downloaded the given piece
        bool have_piece(int index) const
        {
            return has_picker() ? m_picker->have_piece(index) : true;
        }
        bitfield have_pieces() const;

        // called when we learn that we have a piece
        // only once per piece
        void we_have(int index);

        size_t num_have() const
        {
            return has_picker() ? m_picker->num_have() : num_pieces();
        }

        void peer_has(int index)
        {
            if (m_picker.get())
            {
                assert(!is_seed());
                m_picker->inc_refcount(index);
            }
            else
            {
                assert(is_seed());
            }
        }

        size_t num_pieces() const;
        size_t num_free_blocks() const;

        piece_manager& filesystem() { return *m_storage; }
        storage_interface* get_storage();
        void move_storage(const std::string& save_path);
        bool rename_file(const std::string& name);
        void delete_files();

        // unless this returns true, new connections must wait
        // with their initialization.
        bool ready_for_connections() const { return true; }

        boost::uint32_t getAcepted() const { return m_accepted; }
        boost::uint32_t getResuested() const { return m_requested; }
        boost::uint64_t getTransferred() const { return m_transferred; }
        boost::uint8_t  getPriority() const { return m_priority; }

        /**
          * async generate fast resume data and emit alert
         */
        void save_resume_data();

        bool should_check_file() const;

        /**
          * call after transfer checking completed
         */
        void file_checked();
        void start_checking();

        void set_error(error_code const& ec);

        /**
          * add transfer to check queue in session_impl
         */
        void queue_transfer_check();

        /**
          * remove transfer from check queue insession_impl
         */
        void dequeue_transfer_check();

        // --------------------------------------------
        // SERVER MANAGEMENT
        // --------------------------------------------
        /**
          * convert transfer info into announce
         */
        shared_file_entry getAnnounce() const;

        tcp::endpoint const& get_interface() const { return m_net_interface; }

        std::set<peer_connection*> m_connections;

        void on_files_released(int ret, disk_io_job const& j);
        void on_files_deleted(int ret, disk_io_job const& j);
        void on_file_renamed(int ret, disk_io_job const& j);
        void on_storage_moved(int ret, disk_io_job const& j);
        void on_transfer_aborted(int ret, disk_io_job const& j);
        void on_transfer_paused(int ret, disk_io_job const& j);
        void on_save_resume_data(int ret, disk_io_job const& j);
        void on_resume_data_checked(int ret, disk_io_job const& j);
        void on_piece_checked(int ret, disk_io_job const& j);
        void on_piece_verified(int ret, disk_io_job const& j, boost::function<void(int)> f);
        void handle_disk_error(disk_io_job const& j, peer_connection* c = 0);

    private:
        // will initialize the storage and the piece-picker
        void init();
        void bytes_done(transfer_status& st) const;
        void add_failed_bytes(int b);
        int block_bytes_wanted(const piece_block& p) const { return BLOCK_SIZE; }

        void write_resume_data(entry& rd) const;
        void read_resume_data(lazy_entry const& rd);

        // this is the upload and download statistics for the whole transfer.
        // it's updated from all its peers once every second.
        stat m_stat;

        // a back reference to the session
        // this transfer belongs to.
        aux::session_impl& m_ses;
        boost::scoped_ptr<piece_picker> m_picker;

        bool m_announced;   //! transfer announced on server
        // is set to true when the transfer has been aborted.
        bool m_abort;

        bool m_paused;
        bool m_sequential_download;

        int m_sequence_number;

        // the network interface all outgoing connections
        // are opened through
        tcp::endpoint m_net_interface;
        std::string   m_save_path;     //!< file save path
        // the number of seconds we've been in upload mode
        unsigned int m_upload_mode_time;

        // determines the storage state for this transfer.
        storage_mode_t m_storage_mode;

        // the state of this transfer (queued, checking, downloading, etc.)
        transfer_status::state_t m_state;

        // this means we haven't verified the file content
        // of the files we're seeding. the m_verified bitfield
        // indicates which pieces have been verified and which
        // haven't
        bool m_seed_mode;

        // set to true when this transfer may not download anything
        bool m_upload_mode;

        // if this is true, libed2k may pause and resume
        // this transfer depending on queuing rules. Transfers
        // started with auto_managed flag set may be added in
        // a paused state in case there are no available
        // slots.
        bool m_auto_managed;

        int m_complete;
        int m_incomplete;

        policy m_policy;

        // used for compatibility with piece_manager,
        // may store invalid data
        // should store valid file path
        boost::intrusive_ptr<transfer_info> m_info;

        boost::uint32_t m_accepted;
        boost::uint32_t m_requested;
        boost::uint64_t m_transferred;
        boost::uint8_t  m_priority;

        // all time totals of uploaded and downloaded payload
        // stored in resume data
        size_type m_total_uploaded;
        size_type m_total_downloaded;
        bool m_queued_for_checking;
        int m_progress_ppm;

        // the number of bytes that has been
        // downloaded that failed the hash-test
        boost::uint32_t m_total_failed_bytes;
        boost::uint32_t m_total_redundant_bytes;

        // the piece_manager keeps the transfer object
        // alive by holding a shared_ptr to it and
        // the transfer keeps the piece manager alive
        // with this intrusive_ptr. This cycle is
        // broken when transfer::abort() is called
        // Then the transfer releases the piece_manager
        // and when the piece_manager is complete with all
        // outstanding disk io jobs (that keeps
        // the piece_manager alive) it will destruct
        // and release the transfer file. The reason for
        // this is that the transfer_info is used by
        // the piece_manager, and stored in the
        // torrent, so the torrent cannot destruct
        // before the piece_manager.
        boost::intrusive_ptr<piece_manager> m_owning_storage;

        // this is a weak (non owninig) pointer to
        // the piece_manager. This is used after the torrent
        // has been aborted, and it can no longer own
        // the object.
        piece_manager* m_storage;

        duration_timer m_minute_timer;

        /**
          * previously saved resume data
         */
        std::vector<char>  m_resume_data;
        lazy_entry m_resume_entry;

        /**
          * current error on this transfer
         */
        error_code m_error;
    };

    extern shared_file_entry transfer2sfe(const std::pair<md4_hash, boost::shared_ptr<transfer> >& tran);
}

#endif
