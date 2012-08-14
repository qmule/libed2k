
#ifndef __LIBED2K_CONSTANTS__
#define __LIBED2K_CONSTANTS__

#include <boost/cstdint.hpp>
#include <libed2k/types.hpp>

namespace libed2k {

    const fsize_t PIECE_SIZE = 9728000ull;
    const fsize_t BLOCK_SIZE = 16*1024;
    const boost::uint32_t DISK_BLOCK_SIZE = 16*1024; // gcd(PIECE_SIZE, BLOCK_SIZE) / 2 = 10240;
    const size_t HIGHEST_LOWID_ED2K = 16777216;

    const size_t READ_HANDLER_MAX_SIZE = 256;
    const size_t WRITE_HANDLER_MAX_SIZE = 256;
    const int LIBED2K_SERVER_CONN_MAX_SIZE = 250000;    //!< max packet body size for server connection

    enum protocol_type
    {
        STANDARD_EDONKEY = 16
    };

}

#endif
