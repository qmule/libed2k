#include "libed2k/session.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/peer_connection.hpp"
#include "libed2k/server_connection.hpp"
#include "libed2k/transfer_handle.hpp"
#include "libed2k/ip_filter.hpp"

namespace libed2k
{
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

    session_status session::status() const
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        return m_impl->status();
    }

    transfer_handle session::add_transfer(const add_transfer_params& params)
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);

        error_code ec;
        transfer_handle ret = m_impl->add_transfer(params, ec);
        if (ec) throw libed2k_exception(ec);
        return ret;
    }

    void session::post_transfer(const add_transfer_params& params)
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        m_impl->post_transfer(params);
    }

    peer_connection_handle session::add_peer_connection(const net_identifier& np)
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        error_code ec;
        peer_connection_handle ret = m_impl->add_peer_connection(np, ec);
        if (ec) throw libed2k_exception(ec);
        return ret;
    }

    peer_connection_handle session::find_peer_connection(const net_identifier& np) const
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        return m_impl->find_peer_connection_handle(np);
    }

    peer_connection_handle session::find_peer_connection(const md4_hash& hash) const
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        return m_impl->find_peer_connection_handle(hash);
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

    void session::post_search_request(search_request& ro)
    {
        m_impl->m_io_service.post(boost::bind(&aux::session_impl::post_search_request, m_impl, ro));
    }

    void session::post_search_more_result_request()
    {
        m_impl->m_io_service.post(boost::bind(&aux::session_impl::post_search_more_result_request, m_impl));
    }

    void session::post_cancel_search()
    {
        m_impl->m_io_service.post(boost::bind(&aux::session_impl::post_cancel_search, m_impl));
    }

    void session::post_sources_request(const md4_hash& hFile, boost::uint64_t nSize)
    {
        m_impl->m_io_service.post(boost::bind(&aux::session_impl::post_sources_request, m_impl, hFile, nSize));
    }

    void session::listen_on(int port, const char* net_interface /*= 0*/)
    {
        m_impl->m_io_service.post(boost::bind(&aux::session_impl::listen_on, m_impl, port, net_interface));
    }

    bool session::is_listening() const
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        return m_impl->is_listening();
    }

    unsigned short session::listen_port() const
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        return m_impl->listen_port();
    }

    void session::set_settings(const session_settings& settings)
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        m_impl->set_settings(settings);
    }

    session_settings session::settings() const
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        return m_impl->m_settings;
    }

    void session::set_ip_filter(const ip_filter& f)
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        m_impl->set_ip_filter(f);
    }

    const ip_filter& session::get_ip_filter() const
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        return m_impl->get_ip_filter();
    }

    transfer_handle session::find_transfer(const md4_hash & hash) const
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        return m_impl->find_transfer_handle(hash);
    }

    std::vector<transfer_handle> session::get_transfers() const
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        return m_impl->get_transfers();
    }

    std::vector<transfer_handle> session::get_active_transfers() const
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        return m_impl->get_active_transfers();
    }

    void session::remove_transfer(const transfer_handle& h, int options)
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        m_impl->remove_transfer(h, options);
    }

    int session::download_rate_limit() const
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        return m_impl->m_settings.download_rate_limit;
    }

    int session::upload_rate_limit() const
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        return m_impl->m_settings.upload_rate_limit;
    }

    void session::server_connect(const server_connection_parameters& scp)
    {
        m_impl->m_io_service.post(boost::bind(&server_connection::start, m_impl->m_server_connection, scp));
    }

    void session::server_disconnect()
    {
        m_impl->m_io_service.post(boost::bind(&server_connection::stop, m_impl->m_server_connection, boost::asio::error::operation_aborted));
    }

    bool session::server_connection_established() const
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        return m_impl->m_server_connection->connected();
    }

    void session::pause()
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        m_impl->pause();
    }

    void session::resume()
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        m_impl->resume();
    }

    void session::make_transfer_parameters(const std::string& filepath)
    {
        m_impl->m_tpm.make_transfer_params(filepath);
    }

    void session::cancel_transfer_parameters(const std::string& filepath)
    {
        m_impl->m_tpm.cancel_transfer_params(filepath);
    }
    
    void session::start_natpmp() {
        m_impl->m_io_service.post(boost::bind(&aux::session_impl::start_natpmp, m_impl));
    }
    
    void session::start_upnp() {
        m_impl->m_io_service.post(boost::bind(&aux::session_impl::start_upnp, m_impl));
    }

    void session::stop_natpmp()
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        m_impl->stop_natpmp();
    }
    
    void session::stop_upnp()
    {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        m_impl->stop_upnp();
    }

#ifndef LIBED2K_DISABLE_DHT

    void session::start_dht()
    {
      // the state is loaded in load_state()
      boost::mutex::scoped_lock l(m_impl->m_mutex);
      m_impl->start_dht();
    }

    void session::stop_dht()
    {
      boost::mutex::scoped_lock l(m_impl->m_mutex);
      m_impl->stop_dht();
    }

    void session::set_dht_settings(dht_settings const& settings)
    {
      boost::mutex::scoped_lock l(m_impl->m_mutex);
      m_impl->set_dht_settings(settings);
    }

    void session::add_dht_node(std::pair<std::string, int> const& node)
    {
      boost::mutex::scoped_lock l(m_impl->m_mutex);
      m_impl->add_dht_node_name(node);
    }

    void session::add_dht_router(std::pair<std::string, int> const& node)
    {
      boost::mutex::scoped_lock l(m_impl->m_mutex);
      m_impl->add_dht_router(node);
    }

    bool session::is_dht_running() const
    {
      boost::mutex::scoped_lock l(m_impl->m_mutex);
      bool r = m_impl->is_dht_running();
      return r;
    }

#endif

    int session::add_port_mapping(protocol_type t, int external_port, int local_port) {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        return m_impl->add_port_mapping(t, external_port, local_port);
    }

    void session::delete_port_mapping(int handle) {
        boost::mutex::scoped_lock l(m_impl->m_mutex);
        m_impl->delete_port_mapping(handle);
    }
}
