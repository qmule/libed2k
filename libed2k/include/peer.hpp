#ifndef __LIBED2K_PEER__
#define __LIBED2K_PEER__

#include <boost/asio.hpp>
#include "types.hpp"

namespace libed2k {

    class peer_connection;

    class peer
    {
    public:
        peer(const tcp::endpoint& ep):
            endpoint(ep), connection(NULL)
        {}

        tcp::endpoint endpoint;

        // if the peer is connected now, this
        // will refer to a valid peer_connection
        peer_connection* connection;

    };

    class peer_entry
    {
    public:
        peer_entry(const std::string& _ip, int _port):
            ip(_ip), port(_port)
        {}

        std::string ip;
        int port;
    };

}

#endif
