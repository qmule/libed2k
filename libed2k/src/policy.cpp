
#include "policy.hpp"
#include "peer.hpp"
#include "session.hpp"
#include "session_impl.hpp"
#include "transfer.hpp"

using namespace libed2k;

policy::policy(transfer* t):
    m_transfer(t), m_num_connect_candidates(0)
{
}

peer* policy::add_peer(const tcp::endpoint& ep)
{
    peer* p = (peer*)m_transfer->session().m_peer_pool.malloc();
    new (p) peer(ep);
    m_peers.push_back(p);

    if (is_connect_candidate(*p))
        ++m_num_connect_candidates;

    return p;
}

void policy::set_connection(peer* p, peer_connection* c)
{
    const bool was_conn_cand = is_connect_candidate(*p);
    p->connection = c;
    if (was_conn_cand) --m_num_connect_candidates;
}

bool policy::connect_one_peer()
{
    // TODO: should be smarter
    return !m_peers.empty() && m_transfer->connect_to_peer(*m_peers.begin());
}


bool policy::is_connect_candidate(peer const& p) const
{
    if (p.connection
//        || p.banned
//        || !p.connectable
//        || (p.seed && finished)
//        || int(p.failcount) >= m_torrent->settings().max_failcount
        )
        return false;

//    aux::session_impl const& ses = m_transfer->session();
//    if (ses.m_port_filter.access(p.port) & port_filter::blocked)
//        return false;

//    if (p.port < 1024 && p.source == peer_info::dht)
//        return false;

    return true;
}
