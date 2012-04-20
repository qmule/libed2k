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

namespace libtorrent {
    class torrent_info;
}

namespace libed2k {

    struct add_transfer_params;
    namespace aux{
        class session_impl;
    }
    class peer;

	// a transfer is a class that holds information
	// for a specific download. It updates itself against
	// the tracker
    class transfer : public boost::enable_shared_from_this<transfer>
    {
    public:

        transfer(aux::session_impl& ses, tcp::endpoint const& net_interface, 
                 int seq, add_transfer_params const& p);
        ~transfer();

        void start();

        bool connect_to_peer(peer* peerinfo);

        bool want_more_peers() const;
        bool try_connect_peer();
        void give_connect_points(int points);

        // the number of peers that belong to this torrent
        int num_peers() const { return (int)m_connections.size(); }
        int num_seeds() const;

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

        void set_sequential_download(bool sd);
        bool is_sequential_download() const { return m_sequential_download; }

        int queue_position() const { return m_sequence_number; }
        int block_size() const { return m_block_size; }

        void second_tick();

        void async_verify_piece(int piece_index, boost::function<void(int)> const&);

        // this is called from the peer_connection
        // each time a piece has failed the hash test
        void piece_finished(int index, int passed_hash_check);

        bool has_picker() const
        {
            return m_picker.get() != 0;
        }
        piece_picker& picker() { return *m_picker; }

        // --------------------------------------------
        // PIECE MANAGEMENT

        // returns true if we have downloaded the given piece
        bool have_piece(int index) const
        {
            return has_picker() ? m_picker->have_piece(index) : true;
        }

        // called when we learn that we have a piece
        // only once per piece
        void we_have(int index);

        int num_have() const
        {
            return has_picker() ? m_picker->num_have() : num_pieces();
        }

        int num_pieces() const;

		// this is called wheh the transfer has completed
		// the download. It will post an event, disconnect
		// all seeds and let the tracker know we're finished.
		void completed();

        // this is called when the transfer has finished. i.e.
        // all the pieces we have not filtered have been downloaded.
        // If no pieces are filtered, this is called first and then
        // completed() is called immediately after it.
        void finished();

        piece_manager& filesystem() { return *m_storage; }

        tcp::endpoint const& get_interface() const { return m_net_interface; }

        std::set<peer_connection*> m_connections;

    private:
        void on_files_released(int ret, disk_io_job const& j);
		void on_piece_verified(int ret, disk_io_job const& j, boost::function<void(int)> f);

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
        int m_block_size;

        // the network interface all outgoing connections
        // are opened through
        tcp::endpoint m_net_interface;

        fs::path m_file_path;

        size_t m_filesize;

        storage_mode_t m_storage_mode;

        // Indicates whether transfer will download anything
        bool m_seed_mode;

        policy m_policy;

        // used for compatibility with piece_manager,
        // may store invalid data
        // should store valid file path
        boost::intrusive_ptr<libtorrent::torrent_info> m_info;

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
