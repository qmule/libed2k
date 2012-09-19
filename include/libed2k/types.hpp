
#ifndef __LIBED2K_TYPES__
#define __LIBED2K_TYPES__

#include <boost/asio.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem.hpp>
#include <boost/pool/pool_alloc.hpp>

namespace boost{
    namespace filesystem{}
}

namespace libtorrent {

    class buffer;
    struct logger;
    struct ptime;
    class listen_failed_alert;
    struct torrent_status;
    struct session_status;
    struct piece_block_progress;
    class stat;
    struct peer_info;

    struct chained_buffer;

    namespace aux{
        struct eh_initializer;
    }

    namespace detail{}
}

namespace libed2k
{
    typedef std::vector<char > socket_buffer; //, boost::pool_allocator<char>

    typedef boost::asio::ip::tcp tcp;
    typedef boost::asio::ip::udp udp;
    //typedef boost::asio::deadline_timer dtimer;
    //typedef boost::posix_time::time_duration time_duration;

    typedef libtorrent::buffer buffer;
    typedef libtorrent::logger logger;
    typedef libtorrent::listen_failed_alert listen_failed_alert;
    typedef libtorrent::aux::eh_initializer eh_initializer;
    typedef libtorrent::session_status session_status;
    typedef libtorrent::piece_block_progress piece_block_progress;

    namespace detail = libtorrent::detail;

    namespace ip = boost::asio::ip;
    namespace fs = boost::filesystem;
    namespace bio = boost::iostreams;
    namespace time = boost::posix_time;
    typedef boost::uint64_t fsize_t;
    typedef boost::int64_t size_type;
    typedef boost::uint64_t unsigned_size_type;

    typedef fs::path fpath;
    typedef fs::recursive_directory_iterator r_dir_itr;
    typedef fs::directory_iterator dir_itr;
    typedef std::string  str_path;
}

#endif
