
#include "libed2k/session.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/peer_connection.hpp"
#include "libed2k/server_connection.hpp"
#include "libed2k/transfer_handle.hpp"
#include "libed2k/file.hpp"

namespace libed2k
{

    void add_transfer_params::dump() const
    {
        DBG("add_transfer_params::dump");
        DBG("file hash: " << file_hash << " all hashes size: " << hashset.size());
        DBG("file path: " << convert_to_native(file_path.string()));
        DBG("file size: " << file_size);
        DBG("accepted: " << accepted <<
            " requested: " << requested <<
            " transf: " << transferred <<
            " priority: " << priority);
    }

    void add_transfer_params::reset()
    {
        file_size = 0;
        seed_mode = false;
        resume_data = 0;
        storage_mode = storage_mode_sparse;
        duplicate_is_error = false;
        accepted = 0;
        requested = 0;
        transferred = 0;
        priority = 0;
    }

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

    void session::server_conn_start()
    {
        m_impl->m_io_service.post(boost::bind(&aux::session_impl::server_conn_start, m_impl));
    }

    void session::server_conn_stop()
    {
        m_impl->m_io_service.post(boost::bind(&aux::session_impl::server_conn_stop, m_impl));
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

    void session::share_file(const std::string& strFilename)
    {
        m_impl->m_io_service.post(boost::bind(&aux::session_impl::share_file, m_impl, strFilename, false));
    }

    void session::unshare_file(const std::string& strFilename)
    {
        m_impl->m_io_service.post(boost::bind(&aux::session_impl::share_file, m_impl, strFilename, true));
    }

    void session::share_dir(const std::string& strRoot, const std::string& strPath, const std::deque<std::string>& excludes)
    {
        m_impl->m_io_service.post(boost::bind(&aux::session_impl::share_dir, m_impl, strRoot, strPath, excludes, false));
    }

    void session::unshare_dir(const std::string& strRoot, const std::string& strPath, const std::deque<std::string>& excludes)
    {
        m_impl->m_io_service.post(boost::bind(&aux::session_impl::share_dir, m_impl, strRoot, strPath, excludes, true));
    }
}
