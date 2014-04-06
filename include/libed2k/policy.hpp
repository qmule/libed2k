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
        policy(transfer* t);
        // this is called once for every peer we get from the server.
        peer* add_peer(const tcp::endpoint& ep, int source, char flags);
        // called when an incoming connection is accepted
        // false means the connection was refused or failed
        bool new_connection(peer_connection& c, int session_time);
        // the given connection was just closed
        void connection_closed(const peer_connection& c, int session_time);

        void ip_filter_updated();

        size_t num_peers() const { return m_peers.size(); }
        void set_connection(peer* p, peer_connection* c);
        void set_failcount(peer* p, int f);

        bool connect_one_peer(int session_time);

        typedef std::deque<peer*> peers_t;

        peers_t::iterator begin_peer() { return m_peers.begin(); }
        peers_t::iterator end_peer() { return m_peers.end(); }
        peers_t::const_iterator begin_peer() const { return m_peers.begin(); }
        peers_t::const_iterator end_peer() const { return m_peers.end(); }

        void erase_peer(peer* p);
        void erase_peer(peers_t::iterator i);

    private:

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

        void update_peer(peer* p, int src, int flags, tcp::endpoint const& remote, char const* destination);
        bool insert_peer(peer* p, peers_t::iterator iter, int flags);

        bool compare_peer_erase(peer const& lhs, peer const& rhs) const;
        bool compare_peer(peer const& lhs, peer const& rhs/*, address const& external_ip*/) const;

        peers_t::iterator find_connect_candidate(int session_time);

        bool is_connect_candidate(peer const& p, bool finished) const;
        bool is_erase_candidate(peer const& p, bool finished) const;
        bool is_force_erase_candidate(peer const& pe) const;
        bool should_erase_immediately(peer const& p) const;

        enum flags_t { force_erase = 1 };
        void erase_peers(int flags = 0);

        peers_t m_peers;
        transfer* m_transfer;

        // since the peer list can grow too large
        // to scan all of it, start at this iterator
        int m_round_robin;

        // The number of peers in our peer list
        // that are connect candidates. i.e. they're
        // not already connected and they have not
        // yet reached their max try count and they
        // have the connectable state (we have a listen
        // port for them).
        int m_num_connect_candidates;

        // the number of seeds in the peer list
        int m_num_seeds;

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
