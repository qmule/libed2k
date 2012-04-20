
#include "session.hpp"
#include "session_impl.hpp"
#include "peer_connection.hpp"
#include "server_connection.hpp"

namespace libed2k {

void session::init(const fingerprint& id, const char* listen_interface,
                   const session_settings& settings)
{
    m_impl.reset(new aux::session_impl(id, listen_interface, settings));
}

transfer_handle session::add_transfer(const add_transfer_params& params)
{
    boost::mutex::scoped_lock l(m_impl->m_mutex);

    error_code ec;
    transfer_handle ret = m_impl->add_transfer(params, ec);
    if (ec) throw libed2k_exception(ec);
    return ret;
}


}
