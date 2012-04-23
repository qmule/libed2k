#ifndef __LIBED2K_PEER__
#define __LIBED2K_PEER__

#include <boost/asio.hpp>
#include "types.hpp"

namespace libed2k {

    class peer_connection;

    class peer
    {
    public:
        tcp::endpoint ip() const { return tcp::endpoint(address, port); }

        libed2k::address address;

        // the port this peer is or was connected on
        boost::uint16_t port;

        // if the peer is connected now, this
        // will refer to a valid peer_connection
        peer_connection* connection;

    };

}

#endif
