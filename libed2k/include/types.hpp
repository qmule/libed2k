
#ifndef __LIBED2K_TYPES__
#define __LIBED2K_TYPES__

#include <boost/asio.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

namespace boost{
    namespace filesystem{}
}

namespace libtorrent {
    class piece_manager;
    class piece_picker;
    struct piece_block;
    struct pending_block;
    struct peer_request;
    struct disk_io_job;
    struct disk_buffer_holder;
    class buffer;
    struct logger;
    struct ptime;
    class listen_failed_alert;
    struct bitfield;

    namespace aux{
        class eh_initializer;
    }
    
}

namespace libed2k {

    typedef boost::asio::ip::tcp tcp;
    typedef boost::asio::ip::udp udp;
    typedef boost::asio::ip::address address;
    typedef boost::asio::deadline_timer dtimer;
    typedef boost::posix_time::time_duration time_duration;

    typedef libtorrent::piece_manager piece_manager;
    typedef libtorrent::piece_picker piece_picker;
    typedef libtorrent::piece_block piece_block;
    typedef libtorrent::pending_block pending_block;
    typedef libtorrent::disk_io_job disk_io_job;
    typedef libtorrent::disk_buffer_holder disk_buffer_holder;
    typedef libtorrent::buffer buffer;
    typedef libtorrent::peer_request peer_request;
    typedef libtorrent::logger logger;
    typedef libtorrent::listen_failed_alert listen_failed_alert;
    typedef libtorrent::aux::eh_initializer eh_initializer;
    typedef libtorrent::bitfield bitfield;

    namespace fs = boost::filesystem;
    namespace bio = boost::iostreams;
    namespace time = boost::posix_time;
    typedef time::ptime ptime;
}

#endif
