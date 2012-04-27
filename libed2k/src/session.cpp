
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

session::~session()
{
    boost::mutex::scoped_lock l(m_impl->m_mutex);

    // if there is at least one destruction-proxy
    // abort the session and let the destructor
    // of the proxy to syncronize
    if (!m_impl.unique()) m_impl->abort();
}

transfer_handle session::add_transfer(const add_transfer_params& params)
{
    boost::mutex::scoped_lock l(m_impl->m_mutex);

    error_code ec;
    transfer_handle ret = m_impl->add_transfer(params, ec);
    if (ec) throw libed2k_exception(ec);
    return ret;
}

std::vector<transfer_handle> session::add_transfer_dir(const fs::path& dir)
{
    boost::mutex::scoped_lock l(m_impl->m_mutex);

    error_code ec;
    std::vector<transfer_handle> ret = m_impl->add_transfer_dir(dir, ec);
    if (ec) throw libed2k_exception(ec);
    return ret;
}

std::auto_ptr<alert> session::pop_alert()
{
    boost::mutex::scoped_lock l(m_impl->m_mutex);
    return m_impl->pop_alert();
}

void session::set_alert_dispatch(boost::function<void(alert const&)> const& fun)
{
    // this function deliberately doesn't acquire the mutex
    return m_impl->set_alert_dispatch(fun);
}

alert const* session::wait_for_alert(time_duration max_wait)
{
    // this function deliberately doesn't acquire the mutex
    return m_impl->wait_for_alert(max_wait);
}

void session::set_alert_mask(boost::uint32_t m)
{
    boost::mutex::scoped_lock l(m_impl->m_mutex);
    m_impl->set_alert_mask(m);
}

size_t session::set_alert_queue_size_limit(size_t queue_size_limit_)
{
    boost::mutex::scoped_lock l(m_impl->m_mutex);
    return m_impl->set_alert_queue_size_limit(queue_size_limit_);
}

void session::post_search_request(search_request& sr)
{
    boost::mutex::scoped_lock l(m_impl->m_mutex);
    m_impl->post_search_request(sr);
}

void session::post_sources_request(const md4_hash& hFile, boost::uint64_t nSize)
{
    boost::mutex::scoped_lock l(m_impl->m_mutex);
    m_impl->post_sources_request(hFile, nSize);
}


}
