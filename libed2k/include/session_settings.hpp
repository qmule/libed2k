#ifndef __LIBED2K_SESSION_SETTINGS__
#define __LIBED2K_SESSION_SETTINGS__

namespace libed2k {

    struct session_settings
    {
        session_settings():
            peer_timeout(120),
            peer_connect_timeout(7)
        {}

        // the number of seconds to wait for any activity on
        // the peer wire before closing the connectiong due
        // to time out.
        int peer_timeout;

        // this is the timeout for a connection attempt. If
        // the connect does not succeed within this time, the
        // connection is dropped. The time is specified in seconds.
        int peer_connect_timeout;

    };

}

#endif
