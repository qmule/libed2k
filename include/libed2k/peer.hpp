#ifndef __LIBED2K_PEER__
#define __LIBED2K_PEER__

#include "libed2k/socket.hpp"

namespace libed2k {

    class peer_connection;

    class peer
    {
    public:
        peer(const tcp::endpoint& ep, bool conn):
            endpoint(ep), connection(NULL), connectable(conn)
        {}

        ip::address address() const { return endpoint.address(); }

        tcp::endpoint endpoint;

        // if the peer is connected now, this
        // will refer to a valid peer_connection
        peer_connection* connection;

        // incoming peers (that don't advertize their listen port)
        // will not be considered connectable. Peers that
        // we have a listen port for will be assumed to be.
        bool connectable;
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
