
#include "server_connection.hpp"
#include "session_impl.hpp"
#include "log.hpp"

using namespace libed2k;

server_connection::server_connection(const aux::session_impl& ses):
    m_name_lookup(ses.m_io_service),
    m_ses(ses)
{
}

void server_connection::start()
{
    const session_settings& settings = m_ses.settings();

    tcp::resolver::query q(settings.server_hostname, "");
    m_name_lookup.async_resolve(
        q, boost::bind(&server_connection::on_name_lookup, self(), _1, _2));
}

void server_connection::close()
{
    m_socket->socket().close();
    m_name_lookup.cancel();
}

void server_connection::on_name_lookup(
    const error_code& error, tcp::resolver::iterator i)
{
    const session_settings& settings = m_ses.settings();

    if (error == boost::asio::error::operation_aborted) return;
    if (error || i == tcp::resolver::iterator())
    {
        LERR_ << "server name: " << settings.server_hostname
              << ", resolve failed: " << error;
        return;
    }

    m_target = *i;
    m_target.port(settings.server_port);

    LDBG_ << "server name resolved: " << libtorrent::print_endpoint(m_target);

    error_code ec;
    m_socket.reset(new base_socket(m_ses.m_io_service));

    m_socket->socket().async_connect(
        m_target, boost::bind(&server_connection::on_connection_complete, self(), _1));
}

void server_connection::on_connection_complete(error_code const& error)
{
    if (error)
    {
        LERR_ << "connection to: " << libtorrent::print_endpoint(m_target)
              << ", failed: " << error;
        return;
    }

    LDBG_ << "connect to server:" << libtorrent::print_endpoint(m_target)
          << ", successfully";
}
