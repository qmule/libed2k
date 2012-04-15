
#include "session.hpp"
#include "session_impl.hpp"
#include "peer_connection.hpp"

namespace libed2k {

void session::init(const fingerprint& id, int listen_port, const char* listen_interface,
                   const std::string& logpath)
{
    m_impl.reset(new aux::session_impl(id, listen_port, listen_interface, logpath));
}

}
