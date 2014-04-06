
#include "libed2k/config.hpp"
#include "libed2k/policy.hpp"
#include "libed2k/peer.hpp"
#include "libed2k/session.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/transfer.hpp"
#include "libed2k/peer_connection.hpp"
#include "libed2k/broadcast_socket.hpp"

using namespace libed2k;

struct match_peer_endpoint
{
    match_peer_endpoint(tcp::endpoint const& ep): m_ep(ep)
    {}

    bool operator()(const peer* p) const
    { return p->endpoint == m_ep; }

    tcp::endpoint const& m_ep;
};

struct match_peer_connection
{
    match_peer_connection(const peer_connection& c) : m_conn(c) {}

    bool operator()(const peer* p) const
    { return p->connection == &m_conn; }

    const peer_connection& m_conn;
};

// returns the rank of a peer's source. We have an affinity
// to connecting to peers with higher rank. This is to avoid
// problems when our peer list is diluted by stale peers from
// the resume data for instance
int libed2k::source_rank(int source_bitmask)
{
    int ret = 0;
    if (source_bitmask & peer_info::tracker) ret |= 1 << 5;
    if (source_bitmask & peer_info::lsd) ret |= 1 << 4;
    if (source_bitmask & peer_info::dht) ret |= 1 << 3;
    if (source_bitmask & peer_info::pex) ret |= 1 << 2;
    return ret;
}

policy::policy(transfer* t):
    m_transfer(t), m_round_robin(0),
    m_num_connect_candidates(0), m_num_seeds(0), m_finished(false)
{
}

peer* policy::add_peer(const tcp::endpoint& ep, int source, char flags)
{
    aux::session_impl& ses = m_transfer->session();

    // if the IP is blocked, don't add it
    if (ses.m_ip_filter.access(ep.address()) & ip_filter::blocked)
    {
        error_code ec;
        DBG("blocked peer: " << ep.address().to_string(ec));
        ses.m_alerts.post_alert_should(peer_blocked_alert(m_transfer->handle(), ep.address()));
        return NULL;
    }

    peers_t::iterator iter;
    peer* p = 0;

    bool found = false;
    if (ses.settings().allow_multiple_connections_per_ip)
    {
        std::pair<peers_t::iterator, peers_t::iterator> range =
            find_peers(ep.address());
        iter = std::find_if(range.first, range.second, match_peer_endpoint(ep));

        if (iter != range.second) found = true;
    }
    else
    {
        iter = std::lower_bound(m_peers.begin(), m_peers.end(),
                                ep.address(), peer_address_compare());

        if (iter != m_peers.end() && (*iter)->address() == ep.address())
            found = true;
    }

    if (!found)
    {
        // we don't have any info about this peer.
        // add a new entry

        p = (peer*)ses.m_ipv4_peer_pool.malloc();
        if (p == 0) return NULL;
        ses.m_ipv4_peer_pool.set_next_size(500);
        new (p) peer(ep, true, source);

        if (!insert_peer(p, iter, flags))
        {
            m_transfer->session().m_ipv4_peer_pool.destroy(p);
            return 0;
        }
        else
        {
            p = *iter;
            update_peer(p, source, flags, ep, 0);
        }
    }

    return p;
}

bool policy::new_connection(peer_connection& c, int session_time)
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
                LIBED2K_ASSERT(i->connection == 0);
            }
            else if (!i->connection->is_connecting() || c.is_local())
            {
                c.disconnect(errors::duplicate_peer_id);
                return false;
            }
            else
            {
                i->connection->disconnect(errors::duplicate_peer_id);
                LIBED2K_ASSERT(i->connection == 0);
            }
        }

        if (is_connect_candidate(*i, m_finished))
        {
            m_num_connect_candidates--;
            LIBED2K_ASSERT(m_num_connect_candidates >= 0);
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

        peer* p = (peer*)ses.m_ipv4_peer_pool.malloc();
        if (p == 0) return false;
        ses.m_ipv4_peer_pool.set_next_size(500);
        new (p) peer(c.remote(), false, 0);

        iter = m_peers.insert(iter, p);
        if (m_round_robin >= iter - m_peers.begin()) ++m_round_robin;
        i = *iter;
        i->source = peer_info::incoming;
    }

    c.set_peer(i);

    // TODO: restore transfer rate limits

    i->connection = &c;
    LIBED2K_ASSERT(i->connection);
    if (!c.fast_reconnect())
        i->last_connected = session_time;

    // this cannot be a connect candidate anymore, since i->connection is set
    LIBED2K_ASSERT(!is_connect_candidate(*i, m_finished));
    //LIBED2K_ASSERT(has_connection(&c));
    m_transfer->state_updated();
    return true;
}

bool policy::insert_peer(peer* p, peers_t::iterator iter, int flags)
{
    LIBED2K_ASSERT(p);
    //LIBED2K_ASSERT(p->in_use);

    int max_peerlist_size = m_transfer->is_paused() ?
        m_transfer->settings().max_paused_peerlist_size :
        m_transfer->settings().max_peerlist_size;

    if (max_peerlist_size && int(m_peers.size()) >= max_peerlist_size)
    {
        if (p->source == peer_info::resume_data) return false;

        erase_peers();
        if (int(m_peers.size()) >= max_peerlist_size)
            return 0;

        // since some peers were removed, we need to
        // update the iterator to make it valid again
#if LIBED2K_USE_I2P
        if (p->is_i2p_addr)
        {
            iter = std::lower_bound(
                m_peers.begin(), m_peers.end(), p->dest(), peer_address_compare());
        }
        else
#endif
            iter = std::lower_bound(
                m_peers.begin(), m_peers.end(), p->address(), peer_address_compare());
    }

    iter = m_peers.insert(iter, p);

    if (m_round_robin >= iter - m_peers.begin()) ++m_round_robin;

#ifndef LIBED2K_DISABLE_ENCRYPTION
    //if (flags & 0x01) p->pe_support = true;
#endif
    if (flags & 0x02)
    {
        p->seed = true;
        ++m_num_seeds;
    }
    //if (flags & 0x04)
    //    p->supports_utp = true;
    //if (flags & 0x08)
    //    p->supports_holepunch = true;

#ifndef LIBED2K_DISABLE_GEO_IP
    //int as = m_transfer->session().as_for_ip(p->address());
#ifdef LIBED2K_DEBUG
    //p->inet_as_num = as;
#endif
    //p->inet_as = m_transfer->session().lookup_as(as);
#endif
    if (is_connect_candidate(*p, m_finished))
        ++m_num_connect_candidates;

    m_transfer->state_updated();

    return true;
}

void policy::update_peer(peer* p, int src, int flags, const tcp::endpoint& remote, char const* destination)
{
    bool was_conn_cand = is_connect_candidate(*p, m_finished);

    p->connectable = true;

    LIBED2K_ASSERT(p->address() == remote.address());
    //p->port = remote.port();
    p->source |= src;

    // if this peer has failed before, decrease the
    // counter to allow it another try, since somebody
    // else is appearantly able to connect to it
    // only trust this if it comes from the tracker
    if (p->failcount > 0 && src == peer_info::tracker)
        --p->failcount;

    // if we're connected to this peer
    // we already know if it's a seed or not
    // so we don't have to trust this source
    if ((flags & 0x02) && !p->connection)
    {
        if (!p->seed) ++m_num_seeds;
        p->seed = true;
    }
    //if (flags & 0x04)
    //    p->supports_utp = true;
    //if (flags & 0x08)
    //    p->supports_holepunch = true;

#if defined LIBED2K_VERBOSE_LOGGING || defined LIBED2K_LOGGING
    if (p->connection)
    {
        // this means we're already connected
        // to this peer. don't connect to
        // it again.

        error_code ec;
        char hex_pid[41];
        to_hex((char*)&p->connection->pid()[0], 20, hex_pid);
        char msg[200];
        snprintf(msg, 200, "already connected to peer: %s %s"
                 , print_endpoint(remote).c_str(), hex_pid);
        m_transfer->debug_log(msg);

        LIBED2K_ASSERT(p->connection->associated_torrent().lock().get() == m_transfer);
    }
#endif

    if (was_conn_cand != is_connect_candidate(*p, m_finished))
    {
        m_num_connect_candidates += was_conn_cand ? -1 : 1;
        if (m_num_connect_candidates < 0) m_num_connect_candidates = 0;
    }
}

// this is called whenever a peer connection is closed
void policy::connection_closed(const peer_connection& c, int session_time)
{
    peer* p = c.get_peer();

    // if we couldn't find the connection in our list, just ignore it.
    if (p == 0) return;

    // web seeds are special, they're not connected via the peer list
    // so they're not kept in m_peers
    //LIBED2K_ASSERT(p->web_seed ||
    //               std::find_if(m_peers.begin() , m_peers.end() , match_peer_connection(c)) != m_peers.end());

    LIBED2K_ASSERT(p->connection == &c);
    LIBED2K_ASSERT(!is_connect_candidate(*p, m_finished));

    // save transfer rate limits
    //p->upload_rate_limit = c.upload_limit();
    //p->download_rate_limit = c.download_limit();

    p->connection = 0;
    //p->optimistically_unchoked = false;

    // if fast reconnect is true, we won't
    // update the timestamp, and it will remain
    // the time when we initiated the connection.
    if (!c.fast_reconnect())
        p->last_connected = session_time;

    if (c.failed())
    {
        // failcount is a 5 bit value
        if (p->failcount < 31) ++p->failcount;
    }

    if (is_connect_candidate(*p, m_finished))
        ++m_num_connect_candidates;

    // if we're already a seed, it's not as important
    // to keep all the possibly stale peers
    // if we're not a seed, but we have too many peers
    // start weeding the ones we only know from resume
    // data first
    // at this point it may be tempting to erase peers
    // from the peer list, but keep in mind that we might
    // have gotten to this point through new_connection, just
    // disconnecting an old peer, relying on this peer
    // to still exist when we get back there, to assign the new
    // peer connection pointer to it. The peer list must
    // be left intact.

    // if we allow multiple connections per IP, and this peer
    // was incoming and it never advertised its listen
    // port, we don't really know which peer it was. In order
    // to avoid adding one entry for every single connection
    // the peer makes to us, don't save this entry
    if (m_transfer->session().settings().allow_multiple_connections_per_ip &&
        !p->connectable/* && p != m_locked_peer*/)
    {
        erase_peer(p);
    }
}

// disconnects [TODO: and removes] all peers that are now filtered
void policy::ip_filter_updated()
{
    aux::session_impl& ses = m_transfer->session();

    for (peers_t::iterator i = m_peers.begin(); i != m_peers.end();)
    {
        if ((ses.m_ip_filter.access((*i)->address()) & ip_filter::blocked) == 0)
        {
            ++i;
            continue;
        }

        //if (*i == m_locked_peer)
        //{
        //    ++i;
        //    continue;
        //}

        ses.m_alerts.post_alert_should(peer_blocked_alert(m_transfer->handle(), (*i)->address()));

        int current = i - m_peers.begin();
        LIBED2K_ASSERT(current >= 0);
        LIBED2K_ASSERT(m_peers.size() > 0);
        LIBED2K_ASSERT(i != m_peers.end());

        if ((*i)->connection)
        {
            // disconnecting the peer here may also delete the
            // peer_info_struct. If that is the case, just continue
            size_t count = m_peers.size();
            peer_connection* p = (*i)->connection;

            p->disconnect(errors::banned_by_ip_filter);
            // what *i refers to has changed, i.e. cur was deleted
            if (m_peers.size() < count)
            {
                i = m_peers.begin() + current;
                continue;
            }
            LIBED2K_ASSERT((*i)->connection == 0 || (*i)->connection->get_peer() == 0);
        }

        erase_peer(i);
        i = m_peers.begin() + current;
    }
}

void policy::erase_peer(peer* p)
{
    std::pair<peers_t::iterator, peers_t::iterator> range = find_peers(p->address());
    peers_t::iterator iter = std::find_if(range.first, range.second, match_peer_endpoint(p->endpoint));
    if (iter == range.second) return;
    erase_peer(iter);
}

// any peer that is erased from m_peers will be
// erased through this function. This way we can make
// sure that any references to the peer are removed
// as well, such as in the piece picker.
void policy::erase_peer(peers_t::iterator i)
{
    if (m_transfer->has_picker())
        m_transfer->picker().clear_peer(*i);
    if ((*i)->seed) --m_num_seeds;
    if (is_connect_candidate(**i, m_finished))
    {
        LIBED2K_ASSERT(m_num_connect_candidates > 0);
        --m_num_connect_candidates;
    }
    LIBED2K_ASSERT(m_num_connect_candidates < int(m_peers.size()));
    if (m_round_robin > i - m_peers.begin()) --m_round_robin;
    if (m_round_robin >= int(m_peers.size())) m_round_robin = 0;

#if LIBED2K_USE_IPV6
    if ((*i)->is_v6_addr)
    {
        LIBED2K_ASSERT(m_transfer->session().m_ipv6_peer_pool.is_from(static_cast<ipv6_peer*>(*i)));
        m_transfer->session().m_ipv6_peer_pool.destroy(static_cast<ipv6_peer*>(*i));
    }
    else
#endif
#if LIBED2K_USE_I2P
        if ((*i)->is_i2p_addr)
        {
            LIBED2K_ASSERT(m_transfer->session().m_i2p_peer_pool.is_from(static_cast<i2p_peer*>(*i)));
            m_transfer->session().m_i2p_peer_pool.destroy(static_cast<i2p_peer*>(*i));
        }
        else
#endif
        {
            LIBED2K_ASSERT(m_transfer->session().m_ipv4_peer_pool.is_from(*i));
            m_transfer->session().m_ipv4_peer_pool.destroy(*i);
        }
    m_peers.erase(i);
}

void policy::set_connection(peer* p, peer_connection* c)
{
    LIBED2K_ASSERT(c);

    const bool was_conn_cand = is_connect_candidate(*p, m_finished);
    p->connection = c;
    if (was_conn_cand) --m_num_connect_candidates;
}

void policy::set_failcount(peer* p, int f)
{
    const bool was_conn_cand = is_connect_candidate(*p, m_finished);
    p->failcount = f;
    if (was_conn_cand != is_connect_candidate(*p, m_finished))
    {
        if (was_conn_cand) --m_num_connect_candidates;
        else ++m_num_connect_candidates;
    }
}

bool policy::connect_one_peer(int session_time)
{
    LIBED2K_ASSERT(m_transfer->want_more_peers());

    peers_t::iterator i = find_connect_candidate(session_time);
    if (i == m_peers.end()) return false;
    peer& p = **i;

    //LIBED2K_ASSERT(!p.banned);
    LIBED2K_ASSERT(!p.connection);
    LIBED2K_ASSERT(p.connectable);

    LIBED2K_ASSERT(m_finished == m_transfer->is_finished());
    LIBED2K_ASSERT(is_connect_candidate(p, m_finished));
    if (!m_transfer->connect_to_peer(&p))
    {
        // failcount is a 5 bit value
        const bool was_conn_cand = is_connect_candidate(p, m_finished);
        if (p.failcount < 31) ++p.failcount;
        if (was_conn_cand && !is_connect_candidate(p, m_finished))
            --m_num_connect_candidates;
        return false;
    }
    LIBED2K_ASSERT(p.connection);
    LIBED2K_ASSERT(!is_connect_candidate(p, m_finished));
    return true;
}

// this returns true if lhs is a better erase candidate than rhs
bool policy::compare_peer_erase(peer const& lhs, peer const& rhs) const
{
    LIBED2K_ASSERT(lhs.connection == 0);
    LIBED2K_ASSERT(rhs.connection == 0);

    // primarily, prefer getting rid of peers we've already tried and failed
    if (lhs.failcount != rhs.failcount)
        return lhs.failcount > rhs.failcount;

    bool lhs_resume_data_source = lhs.source == peer_info::resume_data;
    bool rhs_resume_data_source = rhs.source == peer_info::resume_data;

    // prefer to drop peers whose only source is resume data
    if (lhs_resume_data_source != rhs_resume_data_source)
        return lhs_resume_data_source > rhs_resume_data_source;

    if (lhs.connectable != rhs.connectable)
        return lhs.connectable < rhs.connectable;

    return lhs.trust_points < rhs.trust_points;
}

// this returns true if lhs is a better connect candidate than rhs
bool policy::compare_peer(peer const& lhs, peer const& rhs/*, address const& external_ip*/) const
{
    // prefer peers with lower failcount
    if (lhs.failcount != rhs.failcount)
        return lhs.failcount < rhs.failcount;

    // Local peers should always be tried first
    bool lhs_local = is_local(lhs.address());
    bool rhs_local = is_local(rhs.address());
    if (lhs_local != rhs_local) return lhs_local > rhs_local;

    if (lhs.last_connected != rhs.last_connected)
        return lhs.last_connected < rhs.last_connected;

    int lhs_rank = source_rank(lhs.source);
    int rhs_rank = source_rank(rhs.source);
    if (lhs_rank != rhs_rank) return lhs_rank > rhs_rank;

#ifndef LIBED2K_DISABLE_GEO_IP
    // don't bias fast peers when seeding
    //if (!m_finished && m_transfer->session().has_asnum_db())
    //{
    //    int lhs_as = lhs.inet_as ? lhs.inet_as->second : 0;
    //    int rhs_as = rhs.inet_as ? rhs.inet_as->second : 0;
    //    if (lhs_as != rhs_as) return lhs_as > rhs_as;
    //}
#endif
    //int lhs_distance = cidr_distance(external_ip, lhs.address());
    //int rhs_distance = cidr_distance(external_ip, rhs.address());
    //if (lhs_distance < rhs_distance) return true;
    return false;
}

policy::peers_t::iterator policy::find_connect_candidate(int session_time)
{
    int candidate = -1;
    int erase_candidate = -1;

    LIBED2K_ASSERT(m_finished == m_transfer->is_finished());

    aux::session_impl& ses = m_transfer->session();
    int min_reconnect_time = ses.settings().min_reconnect_time;
    //address external_ip = m_transfer->session().external_address();

    // don't bias any particular peers when seeding
    //if (m_finished || external_ip == address())
    //{
    //    // set external_ip to a random value, to
    //    // radomize which peers we prefer
    //    address_v4::bytes_type bytes;
    //    std::generate(bytes.begin(), bytes.end(), &random);
    //    external_ip = address_v4(bytes);
    //}

    if (m_round_robin >= int(m_peers.size())) m_round_robin = 0;

#ifndef LIBED2K_DISABLE_DHT
    bool pinged = false;
#endif

    int max_peerlist_size = m_transfer->is_paused() ?
        ses.settings().max_paused_peerlist_size :
        ses.settings().max_peerlist_size;

    for (int iterations = std::min(int(m_peers.size()), 300);
         iterations > 0; --iterations)
    {
        if (m_round_robin >= int(m_peers.size())) m_round_robin = 0;

        peer& pe = *m_peers[m_round_robin];
        //LIBED2K_ASSERT(pe.in_use);
        int current = m_round_robin;

#ifndef LIBED2K_DISABLE_DHT
        // try to send a DHT ping to this peer
        // as well, to figure out if it supports
        // DHT (uTorrent and BitComet doesn't
        // advertise support)
        if (!pinged && !pe.added_to_dht)
        {
            udp::endpoint node(pe.address(), pe.port());
            //m_transfer->session().add_dht_node(node);
            pe.added_to_dht = true;
            pinged = true;
        }
#endif
        // if the number of peers is growing large
        // we need to start weeding.

        if (int(m_peers.size()) >= max_peerlist_size * 0.95 && max_peerlist_size > 0)
        {
            if (is_erase_candidate(pe, m_finished) &&
                (erase_candidate == -1 || !compare_peer_erase(*m_peers[erase_candidate], pe)))
            {
                if (should_erase_immediately(pe))
                {
                    if (erase_candidate > current) --erase_candidate;
                    if (candidate > current) --candidate;
                    erase_peer(m_peers.begin() + current);
                    continue;
                }
                else
                {
                    erase_candidate = current;
                }
            }
        }

        ++m_round_robin;

        if (!is_connect_candidate(pe, m_finished)) continue;

        // compare peer returns true if lhs is better than rhs. In this
        // case, it returns true if the current candidate is better than
        // pe, which is the peer m_round_robin points to. If it is, just
        // keep looking.
        if (candidate != -1 && compare_peer(*m_peers[candidate], pe/*, external_ip*/)) continue;

        if (pe.last_connected && session_time - pe.last_connected < (int(pe.failcount) + 1) * min_reconnect_time)
            continue;

        candidate = current;
    }

    if (erase_candidate > -1)
    {
        if (candidate > erase_candidate) --candidate;
        erase_peer(m_peers.begin() + erase_candidate);
    }

#if defined LIBED2K_LOGGING || defined LIBED2K_VERBOSE_LOGGING
    if (candidate != -1)
    {
        (*m_transfer->session().m_logger) << time_now_string()
                                         << " *** FOUND CONNECTION CANDIDATE ["
            " ip: " << m_peers[candidate]->ip() <<
            " d: " << cidr_distance(external_ip, m_peers[candidate]->address()) <<
            " external: " << external_ip <<
            " t: " << (session_time - m_peers[candidate]->last_connected) <<
            " ]\n";
    }
#endif

    if (candidate == -1) return m_peers.end();
    return m_peers.begin() + candidate;
}

bool policy::is_connect_candidate(peer const& p, bool finished) const
{
    const aux::session_impl& ses = m_transfer->session();

    if (p.connection
        //|| p.banned
        || !p.connectable
        || (p.seed && finished)
        || int(p.failcount) >= ses.settings().max_failcount)
        return false;

    // is there connection to this peer
    boost::intrusive_ptr<peer_connection> c = ses.find_peer_connection(p.endpoint);
    if (c) return false;

    //if (ses.m_port_filter.access(p.port) & port_filter::blocked)
    //    return false;

    // only apply this to peers we've only heard
    // about from the DHT
    //if (ses.m_settings.no_connect_privileged_ports
    //    && p.port < 1024
    //    && p.source == peer_info::dht)
    //    return false;

    return true;
}

bool policy::is_erase_candidate(peer const& pe, bool finished) const
{
    //LIBED2K_ASSERT(pe.in_use);
    //if (&pe == m_locked_peer) return false;
    if (pe.connection) return false;
    if (is_connect_candidate(pe, finished)) return false;

    return (pe.failcount > 0) || (pe.source == peer_info::resume_data);
}

bool policy::is_force_erase_candidate(peer const& pe) const
{
    //if (&pe == m_locked_peer) return false;
    return pe.connection == NULL;
}

void policy::erase_peers(int flags)
{
    int max_peerlist_size = m_transfer->is_paused() ?
        m_transfer->settings().max_paused_peerlist_size :
        m_transfer->settings().max_peerlist_size;

    if (max_peerlist_size == 0 || m_peers.empty()) return;

    int erase_candidate = -1;
    int force_erase_candidate = -1;

    LIBED2K_ASSERT(m_finished == m_transfer->is_finished());

    int round_robin = random() % m_peers.size();

    int low_watermark = max_peerlist_size * 95 / 100;
    if (low_watermark == max_peerlist_size) --low_watermark;

    for (int iterations = (std::min)(int(m_peers.size()), 300);
         iterations > 0; --iterations)
    {
        if (int(m_peers.size()) < low_watermark)
            break;

        if (round_robin == int(m_peers.size())) round_robin = 0;

        peer& pe = *m_peers[round_robin];
        //LIBED2K_ASSERT(pe.in_use);
        int current = round_robin;

        if (is_erase_candidate(pe, m_finished) &&
            (erase_candidate == -1 || !compare_peer_erase(*m_peers[erase_candidate], pe)))
        {
            if (should_erase_immediately(pe))
            {
                if (erase_candidate > current) --erase_candidate;
                if (force_erase_candidate > current) --force_erase_candidate;
                LIBED2K_ASSERT(current >= 0 && current < int(m_peers.size()));
                erase_peer(m_peers.begin() + current);
                continue;
            }
            else
            {
                erase_candidate = current;
            }
        }
        if (is_force_erase_candidate(pe) &&
            (force_erase_candidate == -1 || !compare_peer_erase(*m_peers[force_erase_candidate], pe)))
        {
            force_erase_candidate = current;
        }

        ++round_robin;
    }

    if (erase_candidate > -1)
    {
        LIBED2K_ASSERT(erase_candidate >= 0 && erase_candidate < int(m_peers.size()));
        erase_peer(m_peers.begin() + erase_candidate);
    }
    else if ((flags & force_erase) && force_erase_candidate > -1)
    {
        LIBED2K_ASSERT(force_erase_candidate >= 0 && force_erase_candidate < int(m_peers.size()));
        erase_peer(m_peers.begin() + force_erase_candidate);
    }
}

bool policy::should_erase_immediately(peer const& p) const
{
    //if (&p == m_locked_peer) return false;
    return p.source == peer_info::resume_data;
}
