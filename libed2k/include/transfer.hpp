// ed2k transfer

#ifndef __LIBED2K_TRANSFER__
#define __LIBED2K_TRANSFER__

#include <boost/asio.hpp>

namespace libed2k {

    typedef boost::asio::ip::tcp tcp;

    struct add_transfer_params;

    namespace aux{
        class session_impl;
    }

	// a transfer is a class that holds information
	// for a specific download. It updates itself against
	// the tracker
    class transfer
    {
    public:

        transfer(aux::session_impl& ses, tcp::endpoint const& net_interface, 
                 int block_size, int seq, add_transfer_params const& p);
        ~transfer();

        // this is called when the transfer has metadata.
        // it will initialize the storage and the piece-picker
        void init();

        void start();

        bool is_paused() const;

        void set_sequential_download(bool sd);
        bool is_sequential_download() const { return m_sequential_download; }

        int queue_position() const { return m_sequence_number; }

    private:

        // a back reference to the session
        // this transfer belongs to.
        aux::session_impl& m_ses;

        bool m_paused;
        bool m_sequential_download;

        int m_sequence_number;
    };
}

#endif
