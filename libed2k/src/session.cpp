
#include "session.hpp"

using namespace libed2k;

void session::init(int listen_port, const char* listen_interface,
                   const fingerprint& id, const std::string& logpath)
{
    m_impl.reset(new aux::session_impl(listen_port, listen_interface, id, logpath));
}
