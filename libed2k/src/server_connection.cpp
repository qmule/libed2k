
#include "server_connection.hpp"
#include "session_impl.hpp"

using namespace libed2k;

server_connection::server_connection(const aux::session_impl& ses):
    m_name_lookup(ses.m_io_service),
    m_socket(new tcp::socket(m_ses.m_io_service)),
    m_ses(ses)
{
    error_code ec;
    tcp::socket::non_blocking_io ioc(true);
    m_socket->io_control(ioc, ec);
}

void server_connection::start()
{
    const session_settings& settings = m_ses.settings();

    tcp::resolver::query q(settings.server_hostname);
    m_name_lookup.async_resolve(
        q, boost::bind(&server_connection::on_name_lookup, self(), _1, _2));
}

void server_connection::close()
{
    m_socket->close();
    m_name_lookup.cancel();
}

void server_connection::on_name_lookup(
    const error_code& error, tcp::resolver::iterator i)
{
    if (error == boost::asio::error::operation_aborted) return;
    if (error || i == tcp::resolver::iterator())
    {
        // error
        return;
    }

    const session_settings& settings = m_ses.settings();

    m_target = *i;
    m_target.port(settings.server_port);

    m_socket->async_connect(
        m_target, boost::bind(&server_connection::on_connection_complete, self(), _1));
}

void server_connection::on_connection_complete(error_code const& e)
{
    // start work here
}

