
#ifndef __LIBED2K_CONSTANTS__
#define __LIBED2K_CONSTANTS__

#include <boost/cstdint.hpp>

namespace libed2k {

    const size_t PIECE_SIZE = 9728000ull; // ???
    const boost::uint32_t BLOCK_SIZE = 184320u; //180*1024
    const boost::uint32_t DISK_BLOCK_SIZE = 16384u; // 16*1024
    const size_t HIGHEST_LOWID_ED2K = 16777216;

    const size_t READ_HANDLER_MAX_SIZE = 256;
    const size_t WRITE_HANDLER_MAX_SIZE = 256;
    const int LIBED2K_SERVER_CONN_MAX_SIZE = 250000;    //!< max packet body size for server connection

}

#endif
