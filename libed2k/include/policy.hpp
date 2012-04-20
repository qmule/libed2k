#ifndef __LIBED2K_POLICY__
#define __LIBED2K_POLICY__

#include <deque>

namespace libed2k {

    class peer;
    class peer_connection;
    class transfer;

    class policy
    {
    public:
        void set_connection(peer* p, peer_connection* c);
        bool connect_one_peer();
    private:
        typedef std::deque<peer*> peers_t;

        peers_t m_peers;
        transfer* m_transfer;

        bool is_connect_candidate(peer const& p, bool finished) const;

        // The number of peers in our peer list
        // that are connect candidates. i.e. they're
        // not already connected and they have not
        // yet reached their max try count and they
        // have the connectable state (we have a listen
        // port for them).
        int m_num_connect_candidates;

        // this was the state of the torrent the
        // last time we recalculated the number of
        // connect candidates. Since seeds (or upload
        // only) peers are not connect candidates
        // when we're finished, the set depends on
        // this state. Every time m_torrent->is_finished()
        // is different from this state, we need to
        // recalculate the connect candidates.
        bool m_finished;

    };

}

#endif
