#ifndef __LIBED2K_TYPES__
#define __LIBED2K_TYPES__

#include "libed2k/pch.hpp"
#include <boost/asio.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem.hpp>
#include <boost/pool/pool_alloc.hpp>

namespace boost{
    namespace filesystem{}
}

namespace libed2k
{
    //typedef std::vector<char > socket_buffer; //, boost::pool_allocator<char>

    //typedef boost::asio::ip::tcp tcp;
    //typedef boost::asio::ip::udp udp;

    //namespace fs = boost::filesystem;
    //namespace bio = boost::iostreams;

    typedef boost::uint64_t fsize_t;
    //typedef boost::int64_t size_type;
    //typedef boost::uint64_t unsigned_size_type;

    //typedef fs::path fpath;
}

#endif
