
#include "peer_connection.hpp"
#include "session_impl.hpp"

using namespace libed2k;

peer_connection::peer_connection(aux::session_impl& ses,
                                 boost::weak_ptr<transfer> transfer,
                                 boost::shared_ptr<tcp::socket> s,
                                 const tcp::endpoint& remote, peer* peerinfo):
    m_ses(ses), m_socket(s), m_remote(remote), m_transfer(transfer)
{
}

peer_connection::peer_connection(aux::session_impl& ses,
                                 boost::shared_ptr<tcp::socket> s,
                                 const tcp::endpoint& remote,
                                 peer* peerinfo):
    m_ses(ses), m_socket(s), m_remote(remote)
{
}

peer_connection::~peer_connection()
{
    // TODO: implement
}

void peer_connection::send_buffer(char const* buf, int size, int flags)
{
    if (flags == message_type_request)
        m_requests_in_buffer.push_back(m_send_buffer.size() + size);

    int free_space = m_send_buffer.space_in_last_buffer();
    if (free_space > size) free_space = size;
    if (free_space > 0)
    {
        m_send_buffer.append(buf, free_space);
        size -= free_space;
        buf += free_space;
    }
    if (size <= 0) return;

    std::pair<char*, int> buffer = m_ses.allocate_buffer(size);
    if (buffer.first == 0)
    {
        disconnect(errors::no_memory);
        return;
    }
    TORRENT_ASSERT(buffer.second >= size);
    std::memcpy(buffer.first, buf, size);
    m_send_buffer.append_buffer(buffer.first, buffer.second, size,
                                boost::bind(&aux::session_impl::free_buffer,
                                            boost::ref(m_ses), _1, buffer.second));

    setup_send();
}

void peer_connection::setup_send()
{
}

void peer_connection::on_timeout()
{
    // TODO: implement
}

void peer_connection::disconnect(error_code const& ec, int error)
{
    // TODO: implement
}

void peer_connection::on_connect(int ticket)
{
    // TODO: implement
}

void peer_connection::start()
{
}
