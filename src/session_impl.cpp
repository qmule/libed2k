
#ifndef WIN32
#include <sys/resource.h>
#endif

#include <algorithm>
#include <boost/format.hpp>

#include "libed2k/session_impl.hpp"
#include "libed2k/session.hpp"
#include "libed2k/peer_connection.hpp"
#include "libed2k/socket.hpp"
#include "libed2k/transfer_handle.hpp"
#include "libed2k/transfer.hpp"
#include "libed2k/server_connection.hpp"
#include "libed2k/constants.hpp"
#include "libed2k/log.hpp"
#include "libed2k/alert_types.hpp"
#include "libed2k/file.hpp"
#include "libed2k/util.hpp"

namespace libed2k{
namespace aux{

session_impl_base::session_impl_base(const session_settings& settings) :
    m_io_service(),
    m_abort(false),
    m_settings(settings),
    m_transfers(),
    m_alerts(m_io_service),
    m_tpm(m_alerts, settings.m_known_file)
{
}

session_impl_base::~session_impl_base()
{
    abort();
}

void session_impl_base::abort()
{
    if (m_abort) return;
    m_abort = true;
    m_tpm.stop();
}

void session_impl_base::post_transfer(add_transfer_params const& params)
{
    DBG("session_impl_base::post_transfer");
    error_code ec;
    m_io_service.post(boost::bind(&session_impl_base::add_transfer, this, params, ec));
}

alert const* session_impl_base::wait_for_alert(time_duration max_wait)
{
    return m_alerts.wait_for_alert(max_wait);
}

void session_impl_base::set_alert_mask(boost::uint32_t m)
{
    m_alerts.set_alert_mask(m);
}

size_t session_impl_base::set_alert_queue_size_limit(size_t queue_size_limit_)
{
    return m_alerts.set_alert_queue_size_limit(queue_size_limit_);
}

std::auto_ptr<alert> session_impl_base::pop_alert()
{
    if (m_alerts.pending())
    {
        return m_alerts.get();
    }

    return std::auto_ptr<alert>(0);
}

void session_impl_base::set_alert_dispatch(boost::function<void(alert const&)> const& fun)
{
    m_alerts.set_dispatch_function(fun);
}

session_impl::session_impl(const fingerprint& id, const char* listen_interface,
                           const session_settings& settings):
    session_impl_base(settings),
    m_peer_pool(500),
    m_send_buffers(send_buffer_size),
    m_skip_buffer(4096),
    m_filepool(40),
    m_disk_thread(m_io_service, boost::bind(&session_impl::on_disk_queue, this), m_filepool, BLOCK_SIZE),
    m_half_open(m_io_service),
    m_download_rate(peer_connection::download_channel),
    m_upload_rate(peer_connection::upload_channel),
    m_server_connection(new server_connection(*this)),
    m_next_connect_transfer(m_transfers),
    m_paused(false),
    m_second_timer(seconds(1)),
    m_timer(m_io_service),
    m_last_tick(time_now_hires()),
    m_last_connect_duration(0),
    m_last_announce_duration(0),
    m_user_announced(false),
    m_server_connection_state(SC_OFFLINE),
    m_total_failed_bytes(0),
    m_total_redundant_bytes(0)
{
    DBG("*** create ed2k session ***");

    if (!listen_interface) listen_interface = "0.0.0.0";
    error_code ec;
    m_listen_interface = tcp::endpoint(
        ip::address::from_string(listen_interface, ec), settings.listen_port);
    LIBED2K_ASSERT_VAL(!ec, ec.message());

#ifdef WIN32
    // windows XP has a limit on the number of
    // simultaneous half-open TCP connections
    // here's a table:

    // windows version       half-open connections limit
    // --------------------- ---------------------------
    // XP sp1 and earlier    infinite
    // earlier than vista    8
    // vista sp1 and earlier 5
    // vista sp2 and later   infinite

    // windows release                     version number
    // ----------------------------------- --------------
    // Windows 7                           6.1
    // Windows Server 2008 R2              6.1
    // Windows Server 2008                 6.0
    // Windows Vista                       6.0
    // Windows Server 2003 R2              5.2
    // Windows Home Server                 5.2
    // Windows Server 2003                 5.2
    // Windows XP Professional x64 Edition 5.2
    // Windows XP                          5.1
    // Windows 2000                        5.0

    OSVERSIONINFOEX osv;
    memset(&osv, 0, sizeof(osv));
    osv.dwOSVersionInfoSize = sizeof(osv);
    GetVersionEx((OSVERSIONINFO*)&osv);

    // the low two bytes of windows_version is the actual
    // version.
    boost::uint32_t windows_version
        = ((osv.dwMajorVersion & 0xff) << 16)
        | ((osv.dwMinorVersion & 0xff) << 8)
        | (osv.wServicePackMajor & 0xff);

    // this is the format of windows_version
    // xx xx xx
    // |  |  |
    // |  |  + service pack version
    // |  + minor version
    // + major version

    // the least significant byte is the major version
    // and the most significant one is the minor version
    if (windows_version >= 0x060100)
    {
        // windows 7 and up doesn't have a half-open limit
        m_half_open.limit(0);
    }
    else if (windows_version >= 0x060002)
    {
        // on vista SP 2 and up, there's no limit
        m_half_open.limit(0);
    }
    else if (windows_version >= 0x060000)
    {
        // on vista the limit is 5 (in home edition)
        m_half_open.limit(4);
    }
    else if (windows_version >= 0x050102)
    {
        // on XP SP2 the limit is 10
        m_half_open.limit(9);
    }
    else
    {
        // before XP SP2, there was no limit
        m_half_open.limit(0);
    }
#endif

#if defined LIBED2K_BSD || defined LIBED2K_LINUX
    // ---- auto-cap open files ----

    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
    {
        DBG("max number of open files: " << rl.rlim_cur);

        // deduct some margin for epoll/kqueue, log files,
        // futexes, shared objects etc.
        rl.rlim_cur -= 20;

        // 80% of the available file descriptors should go
        m_settings.connections_limit = (std::min)(m_settings.connections_limit, int(rl.rlim_cur * 8 / 10));
        // 20% goes towards regular files
        m_filepool.resize((std::min)(m_filepool.size_limit(), int(rl.rlim_cur * 2 / 10)));

        DBG("max connections: " << m_settings.connections_limit);
        DBG("max files: " << m_filepool.size_limit());
    }
#endif // LIBED2K_BSD || LIBED2K_LINUX

    m_bandwidth_channel[peer_connection::download_channel] = &m_download_channel;
    m_bandwidth_channel[peer_connection::upload_channel] = &m_upload_channel;

    update_rate_settings();
    update_connections_limit();

    m_io_service.post(boost::bind(&session_impl::on_tick, this, ec));

    m_thread.reset(new boost::thread(boost::ref(*this)));
}

session_impl::~session_impl()
{
    DBG("*** shutting down session ***");
    m_io_service.post(boost::bind(&session_impl::abort, this));

    // we need to wait for the disk-io thread to
    // die first, to make sure it won't post any
    // more messages to the io_service containing references
    // to disk_io_pool inside the disk_io_thread. Once
    // the main thread has handled all the outstanding requests
    // we know it's safe to destruct the disk thread.
    DBG("waiting for disk io thread");
    m_disk_thread.join();

    DBG("waiting for main thread");
    m_thread->join();

    DBG("shutdown complete!");
}

const session_settings& session_impl::settings() const
{
    return m_settings;
}

void session_impl::set_settings(const session_settings& s)
{
    LIBED2K_ASSERT_VAL(s.file_pool_size > 0, s.file_pool_size);

    // if disk io thread settings were changed
    // post a notification to that thread
    bool update_disk_io_thread = false;
    if (m_settings.cache_size != s.cache_size
        || m_settings.cache_expiry != s.cache_expiry
        || m_settings.optimize_hashing_for_speed != s.optimize_hashing_for_speed
        || m_settings.file_checks_delay_per_block != s.file_checks_delay_per_block
        || m_settings.disk_cache_algorithm != s.disk_cache_algorithm
        || m_settings.read_cache_line_size != s.read_cache_line_size
        || m_settings.write_cache_line_size != s.write_cache_line_size
        || m_settings.coalesce_writes != s.coalesce_writes
        || m_settings.coalesce_reads != s.coalesce_reads
        || m_settings.max_queued_disk_bytes != s.max_queued_disk_bytes
        || m_settings.max_queued_disk_bytes_low_watermark != s.max_queued_disk_bytes_low_watermark
        || m_settings.disable_hash_checks != s.disable_hash_checks
        || m_settings.explicit_read_cache != s.explicit_read_cache
#ifndef LIBED2K_DISABLE_MLOCK
        || m_settings.lock_disk_cache != s.lock_disk_cache
#endif
        || m_settings.use_read_cache != s.use_read_cache
        || m_settings.disk_io_write_mode != s.disk_io_write_mode
        || m_settings.disk_io_read_mode != s.disk_io_read_mode
        || m_settings.allow_reordered_disk_operations != s.allow_reordered_disk_operations
        || m_settings.file_pool_size != s.file_pool_size
        || m_settings.volatile_read_cache != s.volatile_read_cache
        || m_settings.no_atime_storage!= s.no_atime_storage
        || m_settings.ignore_resume_timestamps != s.ignore_resume_timestamps
        || m_settings.no_recheck_incomplete_resume != s.no_recheck_incomplete_resume
        || m_settings.low_prio_disk != s.low_prio_disk
        || m_settings.lock_files != s.lock_files)
        update_disk_io_thread = true;

    bool connections_limit_changed = m_settings.connections_limit != s.connections_limit;

    if (m_settings.alert_queue_size != s.alert_queue_size)
        m_alerts.set_alert_queue_size_limit(s.alert_queue_size);

    m_settings = s;

    if (m_settings.cache_buffer_chunk_size <= 0)
        m_settings.cache_buffer_chunk_size = 1;

    update_rate_settings();

    if (connections_limit_changed) update_connections_limit();

    if (m_settings.connection_speed < 0) m_settings.connection_speed = 200;

    if (update_disk_io_thread)
        update_disk_thread_settings();
}

void session_impl::operator()()
{
    // main session thread

    //eh_initializer();

    if (m_listen_interface.port() != 0)
    {
        boost::mutex::scoped_lock l(m_mutex);
        open_listen_port();
        m_server_connection->start();
    }

    m_tpm.start();

    bool stop_loop = false;
    while (!stop_loop)
    {
        error_code ec;
        m_io_service.run(ec);
        if (ec)
        {
            ERR("session_impl::operator()" << ec.message());
            std::string err = ec.message();
        }
        m_io_service.reset();

        boost::mutex::scoped_lock l(m_mutex);
        stop_loop = m_abort;
    }

    boost::mutex::scoped_lock l(m_mutex);
    m_transfers.clear();
}

void session_impl::open_listen_port()
{
    // close the open listen sockets
    DBG("session_impl::open_listen_port()");
    m_listen_sockets.clear();

    // we should only open a single listen socket, that
    // binds to the given interface
    listen_socket_t s = setup_listener(m_listen_interface);

    if (s.sock)
    {
        m_listen_sockets.push_back(s);
        async_accept(s.sock);
    }
}

bool session_impl::listen_on(int port, const char* net_interface)
{
    DBG("listen_on(" << ((net_interface)?net_interface:"null") << ":" << port);
    tcp::endpoint new_interface;

    if (net_interface && std::strlen(net_interface) > 0)
    {
        error_code ec;
        new_interface = tcp::endpoint(ip::address::from_string(net_interface, ec), port);

        if (ec)
        {
            ERR("session_impl::listen_on: " << net_interface << " failed with: " << ec.message());
            return false;
        }
    }
    else
        new_interface = tcp::endpoint(ip::address_v4::any(), port);


    // if the interface is the same and the socket is open
    // don't do anything
    if (new_interface == m_listen_interface
        && !m_listen_sockets.empty()) return true;

    m_listen_interface = new_interface;
    m_settings.listen_port = port;

    server_conn_stop();     // stop server connection and deannounce all transfers
    open_listen_port();     // reset listener
    server_conn_start();    // start server connection, announces will execute in on_tick

    //bool new_listen_address = m_listen_interface.address() != new_interface.address();

    return !m_listen_sockets.empty();
}

bool session_impl::is_listening() const
{
    return !m_listen_sockets.empty();
}

unsigned short session_impl::listen_port() const
{
    if (m_listen_sockets.empty()) return 0;
    return m_listen_sockets.front().external_port;
}

char session_impl::server_connection_state() const
{
    // return flag from session_impl for sync access by main mutex
    return m_server_connection_state;
}

void session_impl::update_disk_thread_settings()
{
    disk_io_job j;
    j.buffer = (char*) new session_settings(m_settings);
    j.action = disk_io_job::update_settings;
    m_disk_thread.add_job(j);
}

void session_impl::async_accept(boost::shared_ptr<ip::tcp::acceptor> const& listener)
{
    boost::shared_ptr<tcp::socket> c(new tcp::socket(m_io_service));
    listener->async_accept(
        *c, bind(&session_impl::on_accept_connection, this, c,
                 boost::weak_ptr<tcp::acceptor>(listener), _1));
}

void session_impl::on_accept_connection(boost::shared_ptr<tcp::socket> const& s,
                                        boost::weak_ptr<ip::tcp::acceptor> listen_socket,
                                        error_code const& e)
{
    boost::shared_ptr<tcp::acceptor> listener = listen_socket.lock();
    if (!listener) return;

    if (e == boost::asio::error::operation_aborted)
    {
        s->close();
        DBG("session_impl::on_accept_connection: abort operation" );
        return;
    }

    if (m_abort)
    {
        DBG("session_impl::on_accept_connection: abort set");
        return;
    }

    error_code ec;
    if (e)
    {
        tcp::endpoint ep = listener->local_endpoint(ec);

        std::string msg =
            "error accepting connection on '" +
            libed2k::print_endpoint(ep) + "' " + e.message();
        DBG(msg);

#ifdef LIBED2K_WINDOWS
        // Windows sometimes generates this error. It seems to be
        // non-fatal and we have to do another async_accept.
        if (e.value() == ERROR_SEM_TIMEOUT)
        {
            async_accept(listener);
            return;
        }
#endif
#ifdef LIBED2K_BSD
        // Leopard sometimes generates an "invalid argument" error. It seems to be
        // non-fatal and we have to do another async_accept.
        if (e.value() == EINVAL)
        {
            async_accept(listener);
            return;
        }
#endif
        if (m_alerts.should_post<mule_listen_failed_alert>())
            m_alerts.post_alert(mule_listen_failed_alert(ep, e));
        return;
    }

    async_accept(listener);
    incoming_connection(s);
}

void session_impl::incoming_connection(boost::shared_ptr<tcp::socket> const& s)
{
    error_code ec;
    // we got a connection request!
    tcp::endpoint endp = s->remote_endpoint(ec);

    if (ec)
    {
        ERR(endp << " <== INCOMING CONNECTION FAILED, could not retrieve remote endpoint " << ec.message());
        return;
    }

    DBG("<== INCOMING CONNECTION " << endp);

    // don't allow more connections than the max setting
    if (num_connections() >= max_connections())
    {
        //TODO: fire alert here

        DBG("number of connections limit exceeded (conns: "
              << num_connections() << ", limit: " << max_connections()
              << "), connection rejected");

        return;
    }

    setup_socket_buffers(*s);

    boost::intrusive_ptr<peer_connection> c(new peer_connection(*this, s, endp, NULL));

    if (!c->is_disconnecting())
    {
        // store connection in map only for real peers
        if (m_server_connection->m_target.address() != endp.address())
        {
            m_connections.insert(c);
        }

        c->start();
    }
}

boost::weak_ptr<transfer> session_impl::find_transfer(const md4_hash& hash)
{
    transfer_map::iterator i = m_transfers.find(hash);

    if (i != m_transfers.end()) return i->second;

    return boost::weak_ptr<transfer>();
}

boost::weak_ptr<transfer> session_impl::find_transfer(const std::string& filename)
{
    transfer_map::iterator itr = m_transfers.begin();

    while(itr != m_transfers.end())
    {
        if (combine_path(itr->second->save_path(), itr->second->name()) == filename)
        {
            return itr->second;
        }

        ++itr;
    }

    return (boost::weak_ptr<transfer>());
}

boost::intrusive_ptr<peer_connection> session_impl::find_peer_connection(const net_identifier& np) const
{
    connection_map::const_iterator itr = std::find_if(
        m_connections.begin(), m_connections.end(), boost::bind(&peer_connection::has_network_point, _1, np));
    if (itr != m_connections.end())  {  return *itr; }
    return boost::intrusive_ptr<peer_connection>();
}

boost::intrusive_ptr<peer_connection> session_impl::find_peer_connection(const md4_hash& hash) const
{
    connection_map::const_iterator itr =
        std::find_if(m_connections.begin(), m_connections.end(), boost::bind(&peer_connection::has_hash, _1, hash));
    if (itr != m_connections.end())  {  return *itr; }
    return boost::intrusive_ptr<peer_connection>();
}

transfer_handle session_impl::find_transfer_handle(const md4_hash& hash)
{
    return transfer_handle(find_transfer(hash));
}

peer_connection_handle session_impl::find_peer_connection_handle(const net_identifier& np)
{
    return peer_connection_handle(find_peer_connection(np), this);
}

peer_connection_handle session_impl::find_peer_connection_handle(const md4_hash& hash)
{
    return peer_connection_handle(find_peer_connection(hash), this);
}

std::vector<transfer_handle> session_impl::get_transfers()
{
    std::vector<transfer_handle> ret;

    for (session_impl::transfer_map::iterator i
        = m_transfers.begin(), end(m_transfers.end());
        i != end; ++i)
    {
        if (i->second->is_aborted()) continue;
        ret.push_back(transfer_handle(i->second));
    }

    return ret;
}

void session_impl::queue_check_transfer(boost::shared_ptr<transfer> const& t)
{
    if (m_abort) return;
    LIBED2K_ASSERT(t->should_check_file());
    LIBED2K_ASSERT(t->state() != transfer_status::checking_files);
    if (m_queued_for_checking.empty()) t->start_checking();
    else t->set_state(transfer_status::queued_for_checking);

    LIBED2K_ASSERT(
        std::find(m_queued_for_checking.begin(), m_queued_for_checking.end(), t) ==
        m_queued_for_checking.end());
    m_queued_for_checking.push_back(t);
}

void session_impl::dequeue_check_transfer(boost::shared_ptr<transfer> const& t)
{
    LIBED2K_ASSERT(t->state() == transfer_status::checking_files
        || t->state() == transfer_status::queued_for_checking);

    if (m_queued_for_checking.empty()) return;

    boost::shared_ptr<transfer> next_check = *m_queued_for_checking.begin();
    check_queue_t::iterator done = m_queued_for_checking.end();
    for (check_queue_t::iterator i = m_queued_for_checking.begin()
        , end(m_queued_for_checking.end()); i != end; ++i)
    {
        LIBED2K_ASSERT(*i == t || (*i)->should_check_file());
        if (*i == t) done = i;
        if (next_check == t || next_check->queue_position() > (*i)->queue_position())
            next_check = *i;
    }
    // only start a new one if we removed the one that is checking
    LIBED2K_ASSERT(done != m_queued_for_checking.end());
    if (done == m_queued_for_checking.end()) return;

    if (next_check != t && t->state() == transfer_status::checking_files)
        next_check->start_checking();

    m_queued_for_checking.erase(done);
}

void session_impl::close_connection(const peer_connection* p, const error_code& ec)
{
    assert(p->is_disconnecting());

    connection_map::iterator i =
        std::find_if(m_connections.begin(), m_connections.end(),
                     boost::bind(&boost::intrusive_ptr<peer_connection>::get, _1) == p);
    if (i != m_connections.end()) m_connections.erase(i);
}

transfer_handle session_impl::add_transfer(add_transfer_params const& params, error_code& ec)
{
    APP("add transfer: {hash: " << params.file_hash << ", path: " << convert_to_native(params.file_path)
        << ", size: " << params.file_size << "}");

    if (is_aborted())
    {
        ec = errors::session_closing;
        return transfer_handle();
    }

    // is the transfer already active?
    boost::shared_ptr<transfer> transfer_ptr = find_transfer(params.file_hash).lock();

    if (transfer_ptr)
    {
        if (!params.duplicate_is_error)
        {
            return transfer_handle(transfer_ptr);
        }

        ec = errors::duplicate_transfer;
        return transfer_handle();
    }

    int queue_pos = 0;
    for (transfer_map::const_iterator i = m_transfers.begin(),
             end(m_transfers.end()); i != end; ++i)
    {
        int pos = i->second->queue_position();
        if (pos >= queue_pos) queue_pos = pos + 1;
    }

    transfer_ptr.reset(new transfer(*this, m_listen_interface, queue_pos, params));
    transfer_ptr->start();

    m_transfers.insert(std::make_pair(params.file_hash, transfer_ptr));

    transfer_handle handle(transfer_ptr);
    m_alerts.post_alert_should(added_transfer_alert(handle));

    return handle;
}

void session_impl::remove_transfer(const transfer_handle& h, int options)
{
    boost::shared_ptr<transfer> tptr = h.m_transfer.lock();
    if (!tptr) return;

    transfer_map::iterator i = m_transfers.find(tptr->hash());

    if (i != m_transfers.end())
    {
        transfer& t = *i->second;
        md4_hash hash = t.hash();

        if (options & session::delete_files)
            t.delete_files();
        t.abort();

        if (i == m_next_connect_transfer) m_next_connect_transfer.inc();

        //t.set_queue_position(-1);
        m_transfers.erase(i);
        m_next_connect_transfer.validate();

        m_alerts.post_alert_should(deleted_transfer_alert(hash));
    }
}

peer_connection_handle session_impl::add_peer_connection(net_identifier np, error_code& ec)
{
    DBG("session_impl::add_peer_connection");

    if (is_aborted())
    {
        ec = errors::session_closing;
        return peer_connection_handle();
    }

    boost::intrusive_ptr<peer_connection> ptr = find_peer_connection(np);

    // peer already connected
    if (ptr)
    {
        DBG("connection exists");
        // already exists
        return peer_connection_handle(ptr, this);
    }

    tcp::endpoint endp(boost::asio::ip::address::from_string(int2ipstr(np.m_nIP)), np.m_nPort);
    boost::shared_ptr<tcp::socket> sock(new tcp::socket(m_io_service));
    setup_socket_buffers(*sock);

    boost::intrusive_ptr<peer_connection> c(
        new peer_connection(*this, boost::weak_ptr<transfer>(), sock, endp, NULL));

    m_connections.insert(c);

    m_half_open.enqueue(boost::bind(&peer_connection::connect, c, _1),
                        boost::bind(&peer_connection::on_timeout, c),
                        libed2k::seconds(m_settings.peer_connect_timeout));

    return (peer_connection_handle(c, this));
}

std::pair<char*, int> session_impl::allocate_buffer(int size)
{
    int num_buffers = (size + send_buffer_size - 1) / send_buffer_size;

    boost::mutex::scoped_lock l(m_send_buffer_mutex);

    return std::make_pair((char*)m_send_buffers.ordered_malloc(num_buffers),
                          num_buffers * send_buffer_size);
}

void session_impl::free_buffer(char* buf, int size)
{
    int num_buffers = size / send_buffer_size;

    boost::mutex::scoped_lock l(m_send_buffer_mutex);
    m_send_buffers.ordered_free(buf, num_buffers);
}

char* session_impl::allocate_disk_buffer(char const* category)
{
    return m_disk_thread.allocate_buffer(category);
}

void session_impl::free_disk_buffer(char* buf)
{
    m_disk_thread.free_buffer(buf);
}

std::string session_impl::buffer_usage()
{
    int send_buffer_capacity = 0;
    int used_send_buffer = 0;
    for (connection_map::const_iterator i = m_connections.begin(),
             end(m_connections.end()); i != end; ++i)
    {
        send_buffer_capacity += (*i)->send_buffer_capacity();
        used_send_buffer += (*i)->send_buffer_size();
    }

    return (boost::format(
                "{disk_queued: %1%, send_buf_size: %2%,"
                " used_send_buf: %3%, send_buf_utilization: %4%}")
            % m_disk_thread.queue_buffer_size()
            % send_buffer_capacity
            % used_send_buffer
            % (used_send_buffer * 100.f / send_buffer_capacity)).str();
}

session_status session_impl::status() const
{
    session_status s;

    s.num_peers = (int)m_connections.size();

    //s.total_redundant_bytes = m_total_redundant_bytes;
    //s.total_failed_bytes = m_total_failed_bytes;

    s.up_bandwidth_queue = m_upload_rate.queue_size();
    s.down_bandwidth_queue = m_download_rate.queue_size();

    s.up_bandwidth_bytes_queue = m_upload_rate.queued_bytes();
    s.down_bandwidth_bytes_queue = m_download_rate.queued_bytes();

    s.has_incoming_connections = false;

    // total
    s.download_rate = m_stat.download_rate();
    s.total_upload = m_stat.total_upload();
    s.upload_rate = m_stat.upload_rate();
    s.total_download = m_stat.total_download();

    // payload
    s.payload_download_rate = m_stat.transfer_rate(stat::download_payload);
    s.total_payload_download = m_stat.total_transfer(stat::download_payload);
    s.payload_upload_rate = m_stat.transfer_rate(stat::upload_payload);
    s.total_payload_upload = m_stat.total_transfer(stat::upload_payload);

    // IP-overhead
    s.ip_overhead_download_rate = m_stat.transfer_rate(stat::download_ip_protocol);
    s.total_ip_overhead_download = m_stat.total_transfer(stat::download_ip_protocol);
    s.ip_overhead_upload_rate = m_stat.transfer_rate(stat::upload_ip_protocol);
    s.total_ip_overhead_upload = m_stat.total_transfer(stat::upload_ip_protocol);

    // tracker
    s.tracker_download_rate = m_stat.transfer_rate(stat::download_tracker_protocol);
    s.total_tracker_download = m_stat.total_transfer(stat::download_tracker_protocol);
    s.tracker_upload_rate = m_stat.transfer_rate(stat::upload_tracker_protocol);
    s.total_tracker_upload = m_stat.total_transfer(stat::upload_tracker_protocol);

    return s;
}

const tcp::endpoint& session_impl::server() const
{
    return m_server_connection->m_target;
}

void session_impl::abort()
{
    if (m_abort) return;
    DBG("*** ABORT CALLED ***");

    // abort the main thread
    session_impl_base::abort();
    error_code ec;
    m_timer.cancel(ec);

    // close the listen sockets
    for (std::list<listen_socket_t>::iterator i = m_listen_sockets.begin(),
             end(m_listen_sockets.end()); i != end; ++i)
    {
        DBG("session_impl::abort: close listen socket" );
        i->sock->close(ec);
    }

    DBG("aborting all transfers (" << m_transfers.size() << ")");
    // abort all transfers
    for (transfer_map::iterator i = m_transfers.begin(),
             end(m_transfers.end()); i != end; ++i)
    {
        i->second->abort();
    }

    DBG("aborting all server requests");
    //m_server_connection.abort_all_requests();
    m_server_connection->close(errors::session_closing);

    for (transfer_map::iterator i = m_transfers.begin();
         i != m_transfers.end(); ++i)
    {
        transfer& t = *i->second;
        t.abort();
    }

    DBG("aborting all connections (" << m_connections.size() << ")");

    // closing all the connections needs to be done from a callback,
    // when the session mutex is not held
    m_io_service.post(boost::bind(&libed2k::connection_queue::close, &m_half_open));

    DBG("connection queue: " << m_half_open.size());
    DBG("without transfers connections size: " << m_connections.size());

    // abort all connections
    while (!m_connections.empty())
    {
        (*m_connections.begin())->disconnect(errors::stopping_transfer);
    }

    DBG("connection queue: " << m_half_open.size());

    m_download_rate.close();
    m_upload_rate.close();

    m_disk_thread.abort();
}

void session_impl::pause()
{
    if (m_paused) return;
    m_paused = true;
    for (transfer_map::iterator i = m_transfers.begin()
        , end(m_transfers.end()); i != end; ++i)
    {
        transfer& t = *i->second;
        t.do_pause();
    }
}

void session_impl::resume()
{
    if (!m_paused) return;
    m_paused = false;
    for (transfer_map::iterator i = m_transfers.begin()
        , end(m_transfers.end()); i != end; ++i)
    {
        transfer& t = *i->second;
        t.do_resume();
    }
}

// this function is called from the disk-io thread
// when the disk queue is low enough to post new
// write jobs to it. It will go through all peer
// connections that are blocked on the disk and
// wake them up
void session_impl::on_disk_queue()
{
}

// used to cache the current time
// every 100 ms. This is cheaper
// than a system call and can be
// used where more accurate time
// is not necessary
extern ptime g_current_time;

initialize_timer::initialize_timer()
{
    g_current_time = time_now_hires();
}

void session_impl::on_tick(error_code const& e)
{
    boost::mutex::scoped_lock l(m_mutex);

    if (m_abort) return;

    if (e == boost::asio::error::operation_aborted) return;

    if (e)
    {
        ERR("*** TICK TIMER FAILED " << e.message());
        ::abort();
        return;
    }

    ptime now = time_now_hires();
    aux::g_current_time = now;

    error_code ec;
    m_timer.expires_from_now(milliseconds(m_settings.tick_interval), ec);
    m_timer.async_wait(bind(&session_impl::on_tick, this, _1));

    m_download_rate.update_quotas(now - m_last_tick);
    m_upload_rate.update_quotas(now - m_last_tick);

    m_last_tick = now;

    // only tick the following once per second
    if (!m_second_timer.expired(now)) return;

    int tick_interval_ms = total_milliseconds(m_second_timer.tick_interval());

    // --------------------------------------------------------------
    // check for incoming connections that might have timed out
    // --------------------------------------------------------------
    // TODO: should it be implemented?

    // --------------------------------------------------------------
    // server connection
    // --------------------------------------------------------------
    // we always check status changing because it check before reconnect processing
    bool server_conn_change_state = m_server_connection_state != m_server_connection->state();

    if (server_conn_change_state)
    {
        m_server_connection_state = m_server_connection->state();
    }

    if (m_server_connection->online()) announce(tick_interval_ms);

    reconnect(tick_interval_ms);

    m_server_connection->check_keep_alive(tick_interval_ms);

    // --------------------------------------------------------------
    // second_tick every transfer
    // --------------------------------------------------------------

    int num_checking = 0;
    int num_queued = 0;
    for (transfer_map::iterator i = m_transfers.begin(); i != m_transfers.end(); ++i)
    {
        transfer& t = *i->second;
        LIBED2K_ASSERT(!t.is_aborted());
        if (t.state() == transfer_status::checking_files) ++num_checking;
        else if (t.state() == transfer_status::queued_for_checking && !t.is_paused()) ++num_queued;
        t.second_tick(m_stat, tick_interval_ms, now);

        // when server connection changes its state to offline - we drop announces status for all transfers
        if (server_conn_change_state)
        {
            if (m_server_connection->offline())
            {
                t.set_announced(false);
            }
        }
    }

    // some people claim that there sometimes can be cases where
    // there is no transfers being checked, but there are transfers
    // waiting to be checked. I have never seen this, and I can't
    // see a way for it to happen. But, if it does, start one of
    // the queued transfers
    if (num_checking == 0 && num_queued > 0)
    {
        LIBED2K_ASSERT(false);
        check_queue_t::iterator i = std::min_element(m_queued_for_checking.begin()
            , m_queued_for_checking.end(), boost::bind(&transfer::queue_position, _1)
            < boost::bind(&transfer::queue_position, _2));

        if (i != m_queued_for_checking.end())
        {
            (*i)->start_checking();
        }
    }

    m_stat.second_tick(tick_interval_ms);

    connect_new_peers();

    // --------------------------------------------------------------
    // disconnect peers when we have too many
    // --------------------------------------------------------------
    // TODO: should it be implemented?
}

void session_impl::connect_new_peers()
{
    // TODO:
    // this loop will "hand out" max(connection_speed, half_open.free_slots())
    // to the transfers, in a round robin fashion, so that every transfer is
    // equally likely to connect to a peer

    int free_slots = m_half_open.free_slots();
    if (!m_transfers.empty() && free_slots > -m_half_open.limit() &&
        num_connections() < m_settings.connections_limit && !m_abort)
    {
        // this is the maximum number of connections we will
        // attempt this tick
        int max_connections_per_second = 10;
        int steps_since_last_connect = 0;
        int num_transfers = int(m_transfers.size());
        m_next_connect_transfer.validate();

        for (;;)
        {
            transfer& t = *m_next_connect_transfer->second;
            if (t.want_more_connections())
            {
                try
                {
                    if (t.try_connect_peer())
                    {
                        --max_connections_per_second;
                        --free_slots;
                        steps_since_last_connect = 0;
                    }
                }
                catch (std::bad_alloc&)
                {
                    // we ran out of memory trying to connect to a peer
                    // lower the global limit to the number of peers
                    // we already have
                    m_settings.connections_limit = num_connections();
                    if (m_settings.connections_limit < 2) m_settings.connections_limit = 2;
                }
            }

            ++m_next_connect_transfer;
            ++steps_since_last_connect;

            // if we have gone two whole loops without
            // handing out a single connection, break
            if (steps_since_last_connect > num_transfers * 2) break;
            // if there are no more free connection slots, abort
            if (free_slots <= -m_half_open.limit()) break;
            // if we should not make any more connections
            // attempts this tick, abort
            if (max_connections_per_second == 0) break;
            // maintain the global limit on number of connections
            if (num_connections() >= m_settings.connections_limit) break;
        }
    }
}

bool session_impl::has_active_transfer() const
{
    for (transfer_map::const_iterator i = m_transfers.begin(), end(m_transfers.end());
         i != end; ++i)
    {
        if (!i->second->is_paused())
        {
            return true;
        }
    }

    return false;
}

void session_impl::setup_socket_buffers(ip::tcp::socket& s)
{
    error_code ec;
    if (m_settings.send_socket_buffer_size)
    {
        tcp::socket::send_buffer_size option(m_settings.send_socket_buffer_size);
        s.set_option(option, ec);
    }
    if (m_settings.recv_socket_buffer_size)
    {
        tcp::socket::receive_buffer_size option(m_settings.recv_socket_buffer_size);
        s.set_option(option, ec);
    }
}

session_impl::listen_socket_t session_impl::setup_listener(
    ip::tcp::endpoint ep, bool v6_only)
{
    DBG("session_impl::setup_listener");
    error_code ec;
    listen_socket_t s;
    s.sock.reset(new tcp::acceptor(m_io_service));
    s.sock->open(ep.protocol(), ec);

    if (ec)
    {
        //ERR("failed to open socket: " << libed2k::print_endpoint(ep)
        //    << ": " << ec.message().c_str());
    }

    s.sock->bind(ep, ec);

    if (ec)
    {
        // post alert

        char msg[200];
        snprintf(msg, 200, "cannot bind to interface \"%s\": %s",
                 libed2k::print_endpoint(ep).c_str(), ec.message().c_str());
        ERR(msg);

        return listen_socket_t();
    }

    s.external_port = s.sock->local_endpoint(ec).port();
    s.sock->listen(5, ec);

    if (ec)
    {
        // post alert

        char msg[200];
        snprintf(msg, 200, "cannot listen on interface \"%s\": %s",
                 libed2k::print_endpoint(ep).c_str(), ec.message().c_str());
        ERR(msg);

        return listen_socket_t();
    }

    // post alert succeeded

    DBG("listening on: " << ep << " external port: " << s.external_port);

    return s;
}

void session_impl::post_search_request(search_request& ro)
{
    m_server_connection->post_search_request(ro);
}

void session_impl::post_search_more_result_request()
{
    m_server_connection->post_search_more_result_request();
}

void session_impl::post_cancel_search()
{
    shared_files_list sl;
    m_server_connection->post_announce(sl);
}

void session_impl::post_sources_request(const md4_hash& hFile, boost::uint64_t nSize)
{
    m_server_connection->post_sources_request(hFile, nSize);
}

void session_impl::announce(int tick_interval_ms)
{
    // check announces available
    if (m_settings.m_announce_timeout == -1)
    {
        return;
    }

    // calculate last
    m_last_announce_duration += tick_interval_ms;

    if (m_last_announce_duration < m_settings.m_announce_timeout*1000)
    {
        return;
    }

    m_last_announce_duration = 0;

    shared_files_list offer_list;

    for (transfer_map::const_iterator i = m_transfers.begin(); i != m_transfers.end(); ++i)
    {
        // we send no more m_max_announces_per_call elements in one packet
        if (offer_list.m_collection.size() >= m_settings.m_max_announces_per_call)
        {
            break;
        }

        transfer& t = *i->second;

        // add transfer to announce list when it has one piece at least and it is not announced yet
        if (!t.is_announced())
        {
            shared_file_entry se = t.getAnnounce();

            if (!se.is_empty())
            {
                offer_list.add(se);
                t.set_announced(true); // mark transfer as announced
            }
        }
    }

    if (offer_list.m_size > 0)
    {
        DBG("session_impl::announce: " << offer_list.m_size);
        m_server_connection->post_announce(offer_list);
    }

    // generate announce for user as transfer when all transfers were announced but user wasn't
    // NOTE - new_announces don't work since server doesn't update users transfer information after first announce
    if ((offer_list.m_size == 0) && !m_user_announced)
    {
        DBG("all transfer probably ware announced - announce user with correct size");
        __file_size total_size;
        total_size.nQuadPart = 0;

        for (transfer_map::const_iterator i = m_transfers.begin(); i != m_transfers.end(); ++i)
        {
            transfer& t = *i->second;
            total_size.nQuadPart += t.size();
        }

        shared_file_entry se;
        se.m_hFile = m_settings.user_agent;

        if (m_server_connection->tcp_flags() & SRV_TCPFLG_COMPRESSION)
        {
            // publishing an incomplete file
            se.m_network_point.m_nIP    = 0xFBFBFBFB;
            se.m_network_point.m_nPort  = 0xFBFB;
        }
        else
        {
            se.m_network_point.m_nIP     = m_server_connection->client_id();
            se.m_network_point.m_nPort   = settings().listen_port;
        }

        // file name is user name with special mark
        se.m_list.add_tag(make_string_tag(std::string("+++USERNICK+++ ") + m_settings.client_name, FT_FILENAME, true));
        se.m_list.add_tag(make_typed_tag(m_server_connection->client_id(), FT_FILESIZE, true));

        // write users size
        if (m_server_connection->tcp_flags() & SRV_TCPFLG_NEWTAGS)
        {
            se.m_list.add_tag(make_typed_tag(total_size.nLowPart, FT_MEDIA_LENGTH, true));
            se.m_list.add_tag(make_typed_tag(total_size.nHighPart, FT_MEDIA_BITRATE, true));
        }
        else
        {
            se.m_list.add_tag(make_typed_tag(total_size.nLowPart, FT_ED2K_MEDIA_LENGTH, false));
            se.m_list.add_tag(make_typed_tag(total_size.nHighPart, FT_ED2K_MEDIA_BITRATE, false));
        }

        offer_list.add(se);
        m_server_connection->post_announce(offer_list);
        m_user_announced = true;
    }
}

void session_impl::reconnect(int tick_interval_ms)
{

    if (m_settings.server_reconnect_timeout == -1)
    {
        // do not execute reconnect - feature turned off
        return;
    }

    if (!m_server_connection->offline())
    {
        m_last_connect_duration = 0;
        return;
    }

    m_last_connect_duration += tick_interval_ms;

    if (m_last_connect_duration < m_settings.server_reconnect_timeout*1000)
    {
        return;
    }

    // perform reconnect
    m_last_connect_duration = 0;
    m_server_connection->start();
}

void session_impl::server_conn_start()
{
    m_server_connection->start();
}

void session_impl::server_conn_stop()
{
    m_server_connection->stop();

    for (transfer_map::iterator i = m_transfers.begin(); i != m_transfers.end(); ++i)
    {
        transfer& t = *i->second;
        t.set_announced(false);
    }

    m_user_announced = false;
}

void session_impl::update_connections_limit()
{
    if (m_settings.connections_limit <= 0)
    {
        m_settings.connections_limit = (std::numeric_limits<int>::max)();
#if LIBED2K_USE_RLIMIT
        rlimit l;
        if (getrlimit(RLIMIT_NOFILE, &l) == 0
            && l.rlim_cur != RLIM_INFINITY)
        {
            m_settings.connections_limit = l.rlim_cur - m_settings.file_pool_size;
            if (m_settings.connections_limit < 5) m_settings.connections_limit = 5;
        }
#endif
    }

    if (num_connections() > m_settings.connections_limit && !m_transfers.empty())
    {
        // if we have more connections that we're allowed, disconnect
        // peers from the transfers so that they are all as even as possible

        int to_disconnect = num_connections() - m_settings.connections_limit;

        int last_average = 0;
        int average = m_settings.connections_limit / m_transfers.size();

        // the number of slots that are unused by transfers
        int extra = m_settings.connections_limit % m_transfers.size();

        // run 3 iterations of this, then we're probably close enough
        for (int iter = 0; iter < 4; ++iter)
        {
            // the number of transfers that are above average
            int num_above = 0;
            for (transfer_map::iterator i = m_transfers.begin(),
                     end(m_transfers.end()); i != end; ++i)
            {
                int num = i->second->num_peers();
                if (num <= last_average) continue;
                if (num > average) ++num_above;
                if (num < average) extra += average - num;
            }

            // distribute extra among the transfers that are above average
            if (num_above == 0) num_above = 1;
            last_average = average;
            average += extra / num_above;
            if (extra == 0) break;
            // save the remainder for the next iteration
            extra = extra % num_above;
        }

        for (transfer_map::iterator i = m_transfers.begin(),
                 end(m_transfers.end()); i != end; ++i)
        {
            int num = i->second->num_peers();
            if (num <= average) continue;

            // distribute the remainder
            int my_average = average;
            if (extra > 0)
            {
                ++my_average;
                --extra;
            }

            int disconnect = (std::min)(to_disconnect, num - my_average);
            to_disconnect -= disconnect;
            i->second->disconnect_peers(
                disconnect, error_code(errors::too_many_connections, get_libed2k_category()));
        }
    }
}

void session_impl::update_rate_settings()
{
    if (m_settings.half_open_limit <= 0)
        m_settings.half_open_limit = (std::numeric_limits<int>::max)();
    m_half_open.limit(m_settings.half_open_limit);

    if (m_settings.download_rate_limit < 0)
        m_settings.download_rate_limit = 0;
    m_download_channel.throttle(m_settings.download_rate_limit);

    if (m_settings.upload_rate_limit < 0)
        m_settings.upload_rate_limit = 0;
    m_upload_channel.throttle(m_settings.upload_rate_limit);
}

}
}
