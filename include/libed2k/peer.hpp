#ifndef __LIBED2K_PEER__
#define __LIBED2K_PEER__

#include "libed2k/socket.hpp"

namespace libed2k {

    class peer_connection;

    // Now it's only ipv4_peer
    class peer
    {
    public:
        peer(const tcp::endpoint& ep, bool conn, int src):
            endpoint(ep), connection(NULL), last_connected(0), next_connect(0),
            connectable(conn), seed(false), failcount(0), fast_reconnects(0),
            trust_points(0), source(src)
#ifndef TORRENT_DISABLE_DHT
            , added_to_dht(false)
#endif
        {}

        ip::address address() const { return endpoint.address(); }
        uint16_t port() const { return endpoint.port(); }

        void set_failcount(peer* p, int f);

        tcp::endpoint endpoint;

        // if the peer is connected now, this
        // will refer to a valid peer_connection
        peer_connection* connection;

        // the time when the peer connected to us
        // or disconnected if it isn't connected right now
        // in number of seconds since session was created
        boost::uint16_t last_connected;

        // the next time to connect this peer 
        boost::uint16_t next_connect;

        // incoming peers (that don't advertize their listen port)
        // will not be considered connectable. Peers that
        // we have a listen port for will be assumed to be.
        bool connectable;

        // this is true if the peer is a seed
        bool seed;

        // the number of failed connection attempts this peer has
        unsigned failcount;

        // the number of times we have allowed a fast
        // reconnect for this peer.
        unsigned fast_reconnects;

        // for every valid piece we receive where this
        // peer was one of the participants, we increase
        // this value. For every invalid piece we receive
        // where this peer was a participant, we decrease
        // this value. If it sinks below a threshold, its
        // considered a bad peer and will be banned.
        signed trust_points; // [-7, 8]

        // a bitmap combining the peer_source flags
        // from peer_info.
        unsigned source;

#ifndef LIBED2K_DISABLE_DHT
        // this is set to true when this peer as been
        // pinged by the DHT
        bool added_to_dht;
#endif
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
