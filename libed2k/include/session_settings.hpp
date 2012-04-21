#ifndef __LIBED2K_SESSION_SETTINGS__
#define __LIBED2K_SESSION_SETTINGS__

namespace libed2k {

    struct session_settings
    {
        session_settings():
            peer_timeout(120),
            peer_connect_timeout(7),
            recv_socket_buffer_size(0),
            send_socket_buffer_size(0),
            server_port(4661),
            peer_port(4662),
            client_name("http://www.aMule.org")
        {
            // prepare empty client hash
            client_hash = md4_hash::m_emptyMD4Hash;
            client_hash[5] = 14;
            client_hash[14] = 111;
        }

        // the number of seconds to wait for any activity on
        // the peer wire before closing the connectiong due
        // to time out.
        int peer_timeout;

        // this is the timeout for a connection attempt. If
        // the connect does not succeed within this time, the
        // connection is dropped. The time is specified in seconds.
        int peer_connect_timeout;

        // sets the socket send and receive buffer sizes
        // 0 means OS default
        int recv_socket_buffer_size;
        int send_socket_buffer_size;

        // ed2k server hostname
        std::string server_hostname;
        // ed2k server port
        int server_port;
        // ed2k peer port for incoming peer connections
        int peer_port;

        std::string client_name; // ed2k client name
        md4_hash client_hash;    // ed2k client hash
    };

}

#endif
