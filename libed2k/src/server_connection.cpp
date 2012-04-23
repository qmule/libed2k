
#include <boost/lexical_cast.hpp>

#include "server_connection.hpp"
#include "session_impl.hpp"
#include "log.hpp"

namespace libed2k{

server_connection::server_connection(aux::session_impl& ses):
    m_nClientId(0),
    m_name_lookup(ses.m_io_service),
    m_keep_alive(ses.m_io_service),
    m_ses(ses)
{
}

void server_connection::start()
{
    const session_settings& settings = m_ses.settings();

    tcp::resolver::query q(settings.server_hostname,
                           boost::lexical_cast<std::string>(settings.server_port));
    m_name_lookup.async_resolve(
        q, boost::bind(&server_connection::on_name_lookup, self(), _1, _2));
}

void server_connection::close()
{
    m_socket->close();
    m_name_lookup.cancel();
    m_keep_alive.cancel();
}

bool server_connection::is_stopped() const
{
    return (!m_socket->socket().is_open());
}

bool server_connection::is_initialized() const
{
    return (m_nClientId != 0);
}

const tcp::endpoint& server_connection::getServerEndpoint() const
{
    return (m_target);
}

void server_connection::on_name_lookup(
    const error_code& error, tcp::resolver::iterator i)
{
    const session_settings& settings = m_ses.settings();

    if (error == boost::asio::error::operation_aborted) return;
    if (error || i == tcp::resolver::iterator())
    {
        ERR("server name: " << settings.server_hostname
            << ", resolve failed: " << error);
        return;
    }

    m_target = *i;

    DBG("server name resolved: " << libtorrent::print_endpoint(m_target));

    error_code ec;

    /**
      * do not use timeout on server connection
     */
    m_socket.reset(new base_socket(m_ses.m_io_service, settings.peer_timeout));

    m_socket->set_unhandled_callback(boost::bind(&server_connection::on_unhandled_packet, this, _1));   //!< handler for unknown packets
    m_socket->add_callback(OP_REJECT,       boost::bind(&server_connection::on_reject,          this, _1));
    m_socket->add_callback(OP_DISCONNECT,   boost::bind(&server_connection::on_disconnect,      this, _1));
    m_socket->add_callback(OP_SERVERMESSAGE,boost::bind(&server_connection::on_server_message,  this, _1));
    m_socket->add_callback(OP_SERVERLIST,   boost::bind(&server_connection::on_server_list,     this, _1));
    m_socket->add_callback(OP_SERVERSTATUS, boost::bind(&server_connection::on_server_status,   this, _1));
    m_socket->add_callback(OP_USERS_LIST,   boost::bind(&server_connection::on_users_list,      this, _1));
    m_socket->add_callback(OP_IDCHANGE,     boost::bind(&server_connection::on_id_change,       this, _1));

    m_socket->do_connect(m_target, boost::bind(&server_connection::on_connection_complete, self(), _1));
}

void server_connection::on_connection_complete(error_code const& error)
{
    if (is_stopped())
    {
        DBG("socket was closed by timeout");
        return;
    }

    if (error)
    {
        ERR("connection to: " << libtorrent::print_endpoint(m_target)
            << ", failed: " << error);
        return;
    }

    m_socket->set_timeout(boost::posix_time::pos_infin);
    m_ses.settings().server_ip = m_target.address().to_v4().to_ulong();

    DBG("connect to server:" << libtorrent::print_endpoint(m_target) << ", successfully");

    const session_settings& settings = m_ses.settings();

    cs_login_request    login;
    //!< generate initial packet to server
    boost::uint32_t nVersion = 0x3c;
    boost::uint32_t nCapability = CAPABLE_AUXPORT | CAPABLE_NEWTAGS | CAPABLE_UNICODE | CAPABLE_LARGEFILES;
    boost::uint32_t nClientVersion  = (3 << 24) | (2 << 17) | (3 << 10) | (1 << 7);

    login.m_hClient                 = settings.client_hash;
    login.m_sNetIdentifier.m_nIP    = 0;
    login.m_sNetIdentifier.m_nPort  = settings.peer_port;

    login.m_list.add_tag(make_string_tag(std::string(settings.client_name), CT_NAME, true));
    login.m_list.add_tag(make_typed_tag(nVersion, CT_VERSION, true));
    login.m_list.add_tag(make_typed_tag(nCapability, CT_SERVER_FLAGS, true));
    login.m_list.add_tag(make_typed_tag(nClientVersion, CT_EMULE_VERSION, true));
    login.m_list.dump();

    // prepare server ping
    m_keep_alive.expires_from_now(boost::posix_time::seconds(settings.server_keep_alive_timeout));
    m_keep_alive.async_wait(boost::bind(&server_connection::write_server_keep_alive, this));

    m_socket->async_read();     // wait incoming messages
    m_socket->do_write(login);  // write login message
}

void server_connection::on_unhandled_packet(const error_code& error)
{

    DBG("receive handle less  packet: " << packetToString(m_socket->context().m_type));

    if (!error)
    {
        m_socket->async_read(); // read next packet
    }
    else
    {
        handle_error(error);
    }
}

void server_connection::on_reject(const error_code& error)
{
    DBG("receive " << packetToString(m_socket->context().m_type));

    if (!error)
    {
        m_socket->async_read();
    }
    else
    {
        handle_error(error);
    }
}

void server_connection::on_disconnect(const error_code& error)
{
    DBG("receive " << packetToString(m_socket->context().m_type));

    if (!error)
    {
        m_socket->async_read();
    }
    else
    {
        handle_error(error);
    }
}

void server_connection::on_server_message(const error_code& error)
{
    DBG("receive " << packetToString(m_socket->context().m_type));

    if (!error)
    {
        server_message smsg;
        m_socket->decode_packet(smsg);

        DBG(smsg.m_strMessage);
        // TODO add alert there
        m_socket->async_read();

    }
    else
    {
        handle_error(error);
    }
}

void server_connection::on_server_list(const error_code& error)
{
    DBG("receive " << packetToString(m_socket->context().m_type));

    if (!error)
    {
        server_list slist;
        m_socket->decode_packet(slist);
        DBG("container size: " << slist.m_collection.size());
        m_socket->async_read();
    }
    else
    {
        handle_error(error);
    }
}

void server_connection::on_server_status(const error_code& error)
{
    DBG("receive " << packetToString(m_socket->context().m_type));

    if (!error)
    {

        server_status sss;
        m_socket->decode_packet(sss);

        DBG("users count: " << sss.m_nUserCount << " files count: " << sss.m_nFilesCount);
        m_socket->async_read();
    }
    else
    {
        handle_error(error);
    }
}

void server_connection::on_users_list(const error_code& error)
{
    DBG("receive " << packetToString(m_socket->context().m_type));

    if (!error)
    {
        m_socket->async_read();
    }
    else
    {
        handle_error(error);
    }
}

void server_connection::on_id_change(const error_code& error)
{
    DBG("receive " << packetToString(m_socket->context().m_type));

    if (!error)
    {
        // simple read new id
        m_socket->decode_packet(m_nClientId);
        DBG("Client ID is: " << m_nClientId);
        m_socket->async_read();
    }
    else
    {
        handle_error(error);
    }

}

void server_connection::handle_error(const error_code& error)
{
    ERR("Error " << error.message());
    m_socket->socket().close();
}

void server_connection::write_server_keep_alive()
{
    // do nothing when server connection stopped
    if (is_stopped())
    {
        DBG("stopped");
        return;
    }

    offer_files_list empty_list;
    DBG("send server ping");

    m_socket->do_write(empty_list);
    m_keep_alive.expires_from_now(boost::posix_time::seconds(m_ses.settings().server_keep_alive_timeout));
    m_keep_alive.async_wait(boost::bind(&server_connection::write_server_keep_alive, this));

}
}
