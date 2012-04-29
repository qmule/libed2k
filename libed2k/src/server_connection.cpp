
#include <boost/lexical_cast.hpp>

#include "server_connection.hpp"
#include "session_impl.hpp"
#include "log.hpp"
#include "alert_types.hpp"

namespace libed2k{

server_connection::server_connection(aux::session_impl& ses):
    m_nClientId(0),
    m_name_lookup(ses.m_io_service),
    m_keep_alive(ses.m_io_service),
    m_ses(ses),
    m_nFilesCount(0),
    m_nUsersCount(0),
    m_socket(ses.m_io_service),
    m_deadline(ses.m_io_service)
{
    m_deadline.expires_at(boost::posix_time::pos_infin);
}

void server_connection::start()
{
    const session_settings& settings = m_ses.settings();

    check_deadline();
    tcp::resolver::query q(settings.server_hostname,
                           boost::lexical_cast<std::string>(settings.server_port));
    m_name_lookup.async_resolve(
        q, boost::bind(&server_connection::on_name_lookup, self(), _1, _2));
}

void server_connection::close()
{
    DBG("server_connection::close()");
    m_socket.close();
    m_deadline.cancel();
    m_name_lookup.cancel();
    m_keep_alive.cancel();
}

bool server_connection::is_stopped() const
{
    return (!m_socket.is_open());
}

bool server_connection::is_initialized() const
{
    return (m_nClientId != 0);
}

const tcp::endpoint& server_connection::getServerEndpoint() const
{
    return (m_target);
}

void server_connection::post_search_request(search_request& sr)
{
    if (!is_stopped())
    {
        do_write(sr);
    }
}

void server_connection::post_sources_request(const md4_hash& hFile, boost::uint64_t nSize)
{
    if (!is_stopped())
    {
        DBG("server_connection::post_sources_request(" << hFile.toString() << ", " << nSize << ")");
        get_file_sources gfs;
        gfs.m_hFile = hFile;
        gfs.m_file_size.nQuadPart = nSize;
        do_write(gfs);
    }
}

void server_connection::post_announce(offer_files_list& offer_list)
{
    if (!is_stopped())
    {
        DBG("server_connection::post_announce: " << offer_list.m_collection.size());
        do_write(offer_list);
    }
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

    // prepare for connect
    // set timeout
    // execute connect
    m_deadline.expires_from_now(boost::posix_time::seconds(settings.peer_connect_timeout));
    m_socket.async_connect(m_target, boost::bind(&server_connection::on_connection_complete, self(), _1));
}

// private callback methods
void server_connection::on_connection_complete(error_code const& error)
{
    DBG("server_connection::on_connection_complete");

    if (is_stopped())
    {
        DBG("socket was closed by timeout");
        return;
    }

    if (error)
    {
        ERR("connection to: " << libtorrent::print_endpoint(m_target)
            << ", failed: " << error);
        close();
        return;
    }

    //m_socket->set_timeout(boost::posix_time::pos_infin);

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
    login.m_sNetIdentifier.m_nPort  = settings.listen_port;

    login.m_list.add_tag(make_string_tag(std::string(settings.client_name), CT_NAME, true));
    login.m_list.add_tag(make_typed_tag(nVersion, CT_VERSION, true));
    login.m_list.add_tag(make_typed_tag(nCapability, CT_SERVER_FLAGS, true));
    login.m_list.add_tag(make_typed_tag(nClientVersion, CT_EMULE_VERSION, true));
    login.m_list.dump();

    // prepare server ping
    m_keep_alive.expires_from_now(boost::posix_time::seconds(settings.server_keep_alive_timeout));
    m_keep_alive.async_wait(boost::bind(&server_connection::write_server_keep_alive, self()));

    do_read();
    do_write(login);      // write login message
}

void server_connection::handle_error(const error_code& error)
{
    ERR("Error " << error.message());
    close();
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

    do_write(empty_list);
    m_keep_alive.expires_from_now(boost::posix_time::seconds(m_ses.settings().server_keep_alive_timeout));
    m_keep_alive.async_wait(boost::bind(&server_connection::write_server_keep_alive, self()));
}

void server_connection::handle_write(const error_code& error, size_t nSize)
{
    if (is_stopped()) return;

    if (!error)
    {
        m_write_order.pop_front();

        if (!m_write_order.empty())
        {
            // set deadline timer
            m_deadline.expires_from_now(boost::posix_time::seconds(m_ses.settings().peer_timeout));

            std::vector<boost::asio::const_buffer> buffers;
            buffers.push_back(boost::asio::buffer(&m_write_order.front().first, header_size));
            buffers.push_back(boost::asio::buffer(m_write_order.front().second));
            boost::asio::async_write(m_socket, buffers, boost::bind(&server_connection::handle_write, self(),
                                boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred));
        }
    }
    else
    {
        handle_error(error);
    }
}

    void server_connection::do_read()
    {
        m_deadline.expires_from_now(boost::posix_time::seconds(m_ses.settings().peer_timeout));
        boost::asio::async_read(m_socket,
                       boost::asio::buffer(&m_in_header, header_size),
                       boost::bind(&server_connection::handle_read_header,
                               self(),
                               boost::asio::placeholders::error,
                               boost::asio::placeholders::bytes_transferred));
    }

    void server_connection::handle_read_header(const error_code& error, size_t nSize)
    {
        if (is_stopped()) return;

        if (!error)
        {
            // we must download body in any case
            // increase internal buffer size if need
            if (m_in_container.size() < m_in_header.m_size - 1)
            {
                m_in_container.resize(m_in_header.m_size - 1);
            }

            boost::asio::async_read(m_socket, boost::asio::buffer(&m_in_container[0], m_in_header.m_size - 1),
                    boost::bind(&server_connection::handle_read_packet, self(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        }
        else
        {
            handle_error(error);
        }
    }

    void server_connection::handle_read_packet(const error_code& error, size_t nSize)
    {
        typedef boost::iostreams::basic_array_source<char> Device;

        if (is_stopped()) return;

        if (!error)
        {
            DBG("server_connection::handle_read_packet(" << error.message() << ", " << nSize << ", " << packetToString(m_in_header.m_type));
            boost::iostreams::stream_buffer<Device> buffer(&m_in_container[0], m_in_header.m_size - 1);
            std::istream in_array_stream(&buffer);
            archive::ed2k_iarchive ia(in_array_stream);

            try
            {
                // dispatch message
                switch (m_in_header.m_type)
                {
                    case OP_REJECT:
                        DBG("ignore");
                        break;
                    case OP_DISCONNECT:
                        DBG("ignore");
                        break;
                    case OP_SERVERMESSAGE:
                    {
                        server_message smsg;
                        ia >> smsg;
                        if (m_ses.m_alerts.should_post<server_message_alert>())
                                m_ses.m_alerts.post_alert(server_message_alert(smsg.m_strMessage));
                        break;
                    }
                    case OP_SERVERLIST:
                    {
                        server_list slist;
                        ia >> slist;

                        break;
                    }
                    case OP_SERVERSTATUS:
                    {
                        server_status sss;
                        ia >> sss;
                        m_nFilesCount = sss.m_nFilesCount;
                        m_nUsersCount = sss.m_nUserCount;

                        if (m_nClientId != 0)
                        {
                            // we already got client id it means
                            // server connection initialized
                            // notyfy session
                            m_ses.server_ready(m_nClientId, m_nFilesCount, m_nUsersCount);
                        }
                        break;
                    }
                    case OP_USERS_LIST:
                        DBG("ignore");
                        break;
                    case OP_IDCHANGE:
                    {
                        ia >> m_nClientId;

                        if (m_nUsersCount != 0)
                        {
                            DBG("users count " << m_nUsersCount);
                            // if we got users count != 0 - at least 1 user must exists on server
                            // (our connection) - server connection initialized
                            // notify session
                            m_ses.server_ready(m_nClientId, m_nFilesCount, m_nUsersCount);
                        }
                        break;
                    }
                    case OP_SERVERIDENT:
                    {
                        server_info_entry se;
                        ia >> se;
                        break;
                    }
                    case OP_FOUNDSOURCES:
                    {
                        found_file_sources fs;
                        ia >> fs;
                        fs.dump();
                        break;
                    }
                    case OP_SEARCHRESULT:
                    {
                        search_file_list sfl;
                        ia >> sfl;
                        m_ses.m_alerts.post_alert_should(search_result_alert(sfl));
                        break;
                    }
                    case OP_CALLBACKREQUESTED:
                        break;
                    default:
                        DBG("ignore unhandled packet");
                        break;
                }

                do_read();

            }
            catch(libed2k_exception& e)
            {
                ERR("packet parse error");
                handle_error(errors::decode_packet_error);
            }
        }
        else
        {
            handle_error(error);
        }
    }


   void server_connection::check_deadline()
   {
       if (is_stopped())
           return;

       // Check whether the deadline has passed. We compare the deadline against
       // the current time since a new asynchronous operation may have moved the
       // deadline before this actor had a chance to run.

       if (m_deadline.expires_at() <= dtimer::traits_type::now())
       {
           DBG("server_connection::check_deadline(): deadline timer expired");

           // The deadline has passed. The socket is closed so that any outstanding
           // asynchronous operations are cancelled.
           close();
           // There is no longer an active deadline. The expiry is set to positive
           // infinity so that the actor takes no action until a new deadline is set.
           m_deadline.expires_at(boost::posix_time::pos_infin);
           boost::system::error_code ignored_ec;
       }

       // Put the actor back to sleep.
       m_deadline.async_wait(boost::bind(&server_connection::check_deadline, self()));
   }
}
