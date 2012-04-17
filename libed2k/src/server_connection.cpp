
#include "server_connection.hpp"
#include "session_impl.hpp"

using namespace libed2k;

server_connection::server_connection(const aux::session_impl& ses):
    m_name_lookup(ses.m_io_service),
    m_ses(ses)
{
}
