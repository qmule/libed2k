
#include "policy.hpp"
#include "peer.hpp"
#include "session.hpp"
#include "session_impl.hpp"
#include "transfer.hpp"
#include "peer_connection.hpp"

using namespace libed2k;

struct match_peer_endpoint
{
    match_peer_endpoint(tcp::endpoint const& ep): m_ep(ep)
	{}

    bool operator()(const peer* p) const
	{ return p->endpoint == m_ep; }

    tcp::endpoint const& m_ep;
};


policy::policy(transfer* t, const std::vector<peer_entry>& peer_list):
    m_transfer(t), m_num_connect_candidates(0)
{
    for (std::vector<peer_entry>::const_iterator pi = peer_list.begin();
         pi != peer_list.end(); ++pi)
    {
        add_peer(tcp::endpoint(ip::address::from_string(pi->ip), pi->port));
    }
}

peer* policy::add_peer(const tcp::endpoint& ep)
{
    peer* p = (peer*)m_transfer->session().m_peer_pool.malloc();
    new (p) peer(ep, true);
    m_peers.push_back(p);

    if (is_connect_candidate(*p))
        ++m_num_connect_candidates;

    return p;
}

bool policy::new_connection(peer_connection& c)
{
    aux::session_impl& ses = m_transfer->session();

    // TODO: check for connection limits

    peers_t::iterator iter;
    peer* i = 0;
    bool found = false;

    if (ses.settings().allow_multiple_connections_per_ip)
    {
        tcp::endpoint remote = c.remote();
        std::pair<peers_t::iterator, peers_t::iterator> range =
            find_peers(remote.address());
        iter = std::find_if(range.first, range.second, match_peer_endpoint(remote));

        if (iter != range.second) found = true;
    }
    else
    {
        iter = std::lower_bound(m_peers.begin(), m_peers.end(),
                                c.remote().address(), peer_address_compare());

        if (iter != m_peers.end() && (*iter)->address() == c.remote().address())
            found = true;
    }

    if (found)
    {
        i = *iter;

        // TODO: check banned

        if (i->connection != 0)
        {
            boost::shared_ptr<tcp::socket> other_socket = i->connection->socket();
            boost::shared_ptr<tcp::socket> this_socket = c.socket();

            error_code ec1;
            error_code ec2;
            bool self_connection =
                other_socket->remote_endpoint(ec2) == this_socket->local_endpoint(ec1)
                ||
                other_socket->local_endpoint(ec2) == this_socket->remote_endpoint(ec1);

            if (ec1)
            {
                c.disconnect(ec1);
                return false;
            }

            if (self_connection)
            {
                c.disconnect(errors::self_connection, 1);
                i->connection->disconnect(errors::self_connection, 1);
                return false;
            }

            // the new connection is a local (outgoing) connection
            // or the current one is already connected
            if (ec2)
            {
                i->connection->disconnect(ec2);
                assert(i->connection == 0);
            }
            else if (!i->connection->is_connecting() || c.is_local())
            {
                c.disconnect(errors::duplicate_peer_id);
                return false;
            }
            else
            {
                i->connection->disconnect(errors::duplicate_peer_id);
                TORRENT_ASSERT(i->connection == 0);
            }
        }

        if (is_connect_candidate(*i))
        {
            m_num_connect_candidates--;
            if (m_num_connect_candidates < 0) m_num_connect_candidates = 0;
        }
    }
    else
    {
        // we don't have any info about this peer.
        // add a new entry
        error_code ec;

        if (int(m_peers.size()) >= ses.settings().max_peerlist_size)
        {
            c.disconnect(errors::too_many_connections);
            return false;
        }

        peer* p = (peer*)ses.m_peer_pool.malloc();
        if (p == 0) return false;
        ses.m_peer_pool.set_next_size(500);
        new (p) peer(c.remote(), false);

        iter = m_peers.insert(iter, p);
        //if (m_round_robin >= iter - m_peers.begin()) ++m_round_robin;
        i = *iter;
    }

    c.set_peer(i);

    // TODO: restore transfer rate limits

    // this cannot be a connect candidate anymore, since i->connection is set
    i->connection = &c;
    return true;
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
