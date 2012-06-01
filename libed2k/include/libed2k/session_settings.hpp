#ifndef __LIBED2K_SESSION_SETTINGS__
#define __LIBED2K_SESSION_SETTINGS__

namespace libed2k {

    class session_settings
    {
    public:
        typedef std::vector<std::pair<std::string, bool> >  fd_list;

        session_settings():
            server_timeout(220),
            peer_timeout(120),
            peer_connect_timeout(7),
            allow_multiple_connections_per_ip(false),
            recv_socket_buffer_size(0),
            send_socket_buffer_size(0),
            server_port(4661),
            listen_port(4662),
            client_name("http://www.aMule.org"),
            server_keep_alive_timeout(200),
            server_ip(0),
            server_reconnect_timeout(5),
            max_peerlist_size(4000),
            download_rate_limit(-1),
            upload_rate_limit(-1)
        {
            // prepare empty client hash
            client_hash = md4_hash::m_emptyMD4Hash;
            client_hash[5] = 14;
            client_hash[14] = 111;
        }

        // the number of seconds to wait for any activity on
        // the server wire before closing the connection due
        // to time out.
        int server_timeout;

        // the number of seconds to wait for any activity on
        // the peer wire before closing the connection due
        // to time out.
        int peer_timeout;

        // this is the timeout for a connection attempt. If
        // the connect does not succeed within this time, the
        // connection is dropped. The time is specified in seconds.
        int peer_connect_timeout;

        // false to not allow multiple connections from the same
        // IP address. true will allow it.
        bool allow_multiple_connections_per_ip;

        // sets the socket send and receive buffer sizes
        // 0 means OS default
        int recv_socket_buffer_size;
        int send_socket_buffer_size;

        // ed2k server hostname
        std::string server_hostname;
        // ed2k server port
        int server_port;
        // ed2k peer port for incoming peer connections
        int listen_port;

        std::string     client_name; // ed2k client name
        int             server_keep_alive_timeout;
        unsigned long int server_ip; // todo: remove
        int             server_reconnect_timeout;   //!< reconnect to server after fail, -1 - do nothing

        // the max number of peers in the peer list
        // per transfer. This is the peers we know
        // about, not necessarily connected to.
        int max_peerlist_size;

        /**
          * session rate limits
          * -1 unlimits
         */
        int download_rate_limit;
        int upload_rate_limit;

        //!< known.met file
        std::string     m_known_file;

        //!< users files and directories
        //!< second parameter true for recursive search and false otherwise
        fd_list m_fd_list;

        md4_hash client_hash;    // ed2k client hash, todo: remove

        /**
          * rules for shared directories tree
         */
        std::deque<rule>    m_rules;
    };

}

#endif
