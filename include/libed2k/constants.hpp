
#ifndef __LIBED2K_CONSTANTS__
#define __LIBED2K_CONSTANTS__

#include <boost/cstdint.hpp>
#include <libed2k/size_type.hpp>

namespace libed2k
{
    const size_type PIECE_SIZE = 9728000ull;
    const size_type BLOCK_SIZE = 256*1024;  // gcd(PIECE_SIZE, BLOCK_SIZE) / 2 = 10240;
    const size_t HIGHEST_LOWID_ED2K = 16777216;
    const size_t MAX_ED2K_PACKET_LEN = 2*BLOCK_SIZE;
    const int LIBED2K_SERVER_CONN_MAX_SIZE = 250000;  //!< max packet body size for server connection

    enum protocol_type
    {
        STANDARD_EDONKEY = 16
    };

}

#endif
