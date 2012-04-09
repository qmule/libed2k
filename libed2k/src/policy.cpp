
#include "policy.hpp"
#include "peer.hpp"

using namespace libed2k;

void policy::set_connection(peer* p, peer_connection* c)
{
    const bool was_conn_cand = is_connect_candidate(*p, m_finished);
    p->connection = c;
    if (was_conn_cand) --m_num_connect_candidates;

}

bool policy::is_connect_candidate(peer const& p, bool finished) const
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
