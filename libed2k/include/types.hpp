
#ifndef __LIBED2K_TYPES__
#define __LIBED2K_TYPES__


namespace libtorrent {
    class piece_manager;
    class piece_picker;
    class logger;
    class error_code;
    class listen_failed_alert;

    namespace aux{
        class eh_initializer;
    }
    
}

namespace libed2k {

    typedef boost::asio::ip::tcp tcp;

    typedef libtorrent::piece_manager piece_manager;
    typedef libtorrent::piece_picker piece_picker;
    typedef libtorrent::piece_block piece_block;
    typedef libtorrent::disk_io_job disk_io_job;
    typedef libtorrent::logger logger;
    typedef libtorrent::error_code error_code;
    typedef libtorrent::listen_failed_alert listen_failed_alert;
    typedef libtorrent::aux::eh_initializer eh_initializer;

    namespace fs = boost::filesystem;
    namespace errors = libtorrent::errors;

}

#endif
