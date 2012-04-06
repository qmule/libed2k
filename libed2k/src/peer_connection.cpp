
#include "peer_connection.hpp"

using namespace libed2k;

peer_connection::peer_connection(aux::session_impl& ses, 
                                 boost::shared_ptr<tcp::socket> s,
                                 const tcp::endpoint& remote):
    m_ses(ses), m_socket(s), m_remote(remote)
{
}


void peer_connection::start()
{
}
