// ed2k transfer

#ifndef __LIBED2K_TRANSFER__
#define __LIBED2K_TRANSFER__

#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/filesystem.hpp>

namespace libtorrent {
    class torrent_info;
    class piece_manager;
    class piece_picker;
}

namespace libed2k {

    typedef boost::asio::ip::tcp tcp;
    namespace fs = boost::filesystem;

    struct add_transfer_params;
    namespace aux{
        class session_impl;
    }

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

        bool is_paused() const;

        void set_sequential_download(bool sd);
        bool is_sequential_download() const { return m_sequential_download; }

        int queue_position() const { return m_sequence_number; }

        bool has_picker() const
        {
            return m_picker.get() != 0;
        }


    private:

        // will initialize the storage and the piece-picker
        void init();

        // a back reference to the session
        // this transfer belongs to.
        aux::session_impl& m_ses;

        boost::scoped_ptr<libtorrent::piece_picker> m_picker;

        bool m_paused;
        bool m_sequential_download;

        int m_sequence_number;

        fs::path m_save_path;

        storage_mode_t m_storage_mode;

        // Indicates whether transfer will download anything
        bool m_seed_mode;

        // used for compatibility with piece_manager,
        // may store invalid data
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
