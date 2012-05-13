#ifndef __LIBED2K_POLICY__
#define __LIBED2K_POLICY__

#include <deque>
#include <vector>

#include "libed2k/peer.hpp"

namespace libed2k {

    class peer;
    class peer_connection;
    class transfer;

    class policy
    {
    public:
        policy(transfer* t, const std::vector<peer_entry>& peer_list);
        // this is called once for every peer we get from the server.
        peer* add_peer(const tcp::endpoint& ep);
        // called when an incoming connection is accepted
        // false means the connection was refused or failed
        bool new_connection(peer_connection& c);

        size_t num_peers() const { return m_peers.size(); }
        void set_connection(peer* p, peer_connection* c);
        bool connect_one_peer();
        int num_connect_candidates() const { return m_num_connect_candidates; }

    private:

        typedef std::deque<peer*> peers_t;

        struct peer_address_compare
        {
            bool operator()(const peer* lhs, const ip::address& rhs) const
            { return lhs->address() < rhs; }

            bool operator()(const ip::address& lhs, const peer* rhs) const
            { return lhs < rhs->address(); }

            bool operator()(const peer* lhs, const peer* rhs) const
            { return lhs->address() < rhs->address(); }
        };

        std::pair<peers_t::iterator, peers_t::iterator> find_peers(const ip::address& a)
        {
            return std::equal_range(
                m_peers.begin(), m_peers.end(), a, peer_address_compare());
        }

        bool is_connect_candidate(peer const& p) const;

        peers_t m_peers;
        transfer* m_transfer;

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
