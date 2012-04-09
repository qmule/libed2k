#ifndef __LIBED2K_PEER__
#define __LIBED2K_PEER__

#include <boost/asio.hpp>

namespace libed2k {

    typedef boost::asio::ip::tcp tcp;
    typedef boost::asio::ip::address address;

    class peer_connection;

    struct peer
    {
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
