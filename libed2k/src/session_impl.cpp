
#include "libtorrent/peer_connection.hpp"

#include "session_impl.hpp"

using namespace libed2k::aux;

session_impl::session_impl(int listen_port, const char* listen_interface,
                           const fingerprint& id, const std::string& logpath):
    m_ipv4_peer_pool(500),
    m_send_buffers(send_buffer_size),
    m_files(40),
    m_io_service(),
	m_host_resolver(m_io_service),
    m_alerts(m_io_service),
    m_disk_thread(m_io_service, boost::bind(&session_impl::on_disk_queue, this),
                  m_files),
    m_half_open(m_io_service),
    m_download_rate(libtorrent::peer_connection::download_channel),
    m_upload_rate(libtorrent::peer_connection::upload_channel),
    m_server_manager(*this),
    m_abort(false),
    m_paused(false),
    m_max_uploads(8),
    m_allowed_upload_slots(8),
    m_max_connections(200)
{
}

void session_impl::on_disk_queue()
{
}
