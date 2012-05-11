// ed2k transfer

#ifndef __LIBED2K_TRANSFER__
#define __LIBED2K_TRANSFER__

#include <set>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/filesystem.hpp>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>

#include "policy.hpp"
#include "types.hpp"
#include "session.hpp"
#include "packet_struct.hpp"

namespace libtorrent {
    class torrent_info;
}

namespace libed2k {

    class add_transfer_params;
    namespace aux{
        class session_impl;
    }
    class peer;
    class server_request;
    class server_response;

	// a transfer is a class that holds information
	// for a specific download. It updates itself against
	// the tracker
    class transfer : public boost::enable_shared_from_this<transfer>
    {
    public:

        transfer(aux::session_impl& ses, tcp::endpoint const& net_interface,
                 int seq, add_transfer_params const& p);
        ~transfer();

        const md4_hash& hash() const { return m_filehash; }
        const hash_set& hashset() const { return m_hashset; }
        size_t filesize() const { return m_filesize; }
        const fs::path& filepath() const { return m_filepath; }

        void start();
        void abort();

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

        bool want_more_connections() const;
        void disconnect_all(const error_code& ec);
        bool try_connect_peer();
        void give_connect_points(int points);

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

        void set_sequential_download(bool sd);
        bool is_sequential_download() const { return m_sequential_download; }

        int queue_position() const { return m_sequence_number; }

        void second_tick();

		// this is called wheh the transfer has completed
		// the download. It will post an event, disconnect
		// all seeds and let the tracker know we're finished.
		void completed();

        // this is called when the transfer has finished. i.e.
        // all the pieces we have not filtered have been downloaded.
        // If no pieces are filtered, this is called first and then
        // completed() is called immediately after it.
        void finished();

        // --------------------------------------------
        // PIECE MANAGEMENT
        // --------------------------------------------
        void async_verify_piece(int piece_index, const md4_hash& hash,
                                const boost::function<void(int)>& fun);

        // this is called from the peer_connection
        // each time a piece has failed the hash test
        void piece_finished(int index, const md4_hash& hash, int passed_hash_check);

        // piece_passed is called when a piece passes the hash check
        // this will tell all peers that we just got his piece
        // and also let the piece picker know that we have this piece
        // so it wont pick it for download
        void piece_passed(int index, const md4_hash& hash);

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

        // called when we learn that we have a piece
        // only once per piece
        void we_have(int index, const md4_hash& hash);

        int num_have() const
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

        int num_pieces() const;

        piece_manager& filesystem() { return *m_storage; }

        // unless this returns true, new connections must wait
        // with their initialization.
        bool ready_for_connections() const { return true; }

        boost::uint32_t getAcepted() const { return m_accepted; }
        boost::uint32_t getResuested() const { return m_requested; }
        boost::uint64_t getTransferred() const { return m_transferred; }
        boost::uint8_t  getPriority() const { return m_priority; }
        size_t          getFilesize() const { return m_filesize; }

        // --------------------------------------------
        // SERVER MANAGEMENT

        /**
          * announce file by session call
         */
        void announce();

        /**
          * convert transfer info into announce
         */
        shared_file_entry getAnnounce() const;

        tcp::endpoint const& get_interface() const { return m_net_interface; }

        std::set<peer_connection*> m_connections;

        void on_disk_error(disk_io_job const& j, peer_connection* c = 0);

    private:
        void on_files_released(int ret, disk_io_job const& j);
		void on_piece_verified(int ret, disk_io_job const& j, boost::function<void(int)> f);
        void on_transfer_aborted(int ret, disk_io_job const& j);

        // will initialize the storage and the piece-picker
        void init();

        // a back reference to the session
        // this transfer belongs to.
        aux::session_impl& m_ses;

        boost::scoped_ptr<piece_picker> m_picker;

        // is set to true when the transfer has been aborted.
        bool m_abort;

        bool m_paused;
        bool m_sequential_download;

        int m_sequence_number;

        // the network interface all outgoing connections
        // are opened through
        tcp::endpoint m_net_interface;

        md4_hash m_filehash;
        hash_set m_hashset;
        fs::path m_filepath;
        size_t m_filesize;
        boost::uint32_t m_file_type;

        storage_mode_t m_storage_mode;

        // Indicates whether transfer will download anything
        bool m_seed_mode;

        policy m_policy;

        // used for compatibility with piece_manager,
        // may store invalid data
        // should store valid file path
        boost::intrusive_ptr<libtorrent::torrent_info> m_info;

        boost::uint32_t m_accepted;
        boost::uint32_t m_requested;
        boost::uint64_t m_transferred;
        boost::uint8_t  m_priority;

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
        // this is that the torrent_info is used by
        // the piece_manager, and stored in the
        // torrent, so the torrent cannot destruct
        // before the piece_manager.
        boost::intrusive_ptr<libtorrent::piece_manager> m_owning_storage;

        // this is a weak (non owninig) pointer to
        // the piece_manager. This is used after the torrent
        // has been aborted, and it can no longer own
        // the object.
        libtorrent::piece_manager* m_storage;

    };
}

#endif
