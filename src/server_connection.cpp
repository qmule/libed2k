#include <boost/lexical_cast.hpp>

#include "libed2k/version.hpp"
#include "libed2k/server_connection.hpp"
#include "libed2k/transfer.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/session.hpp"
#include "libed2k/transfer.hpp"
#include "libed2k/log.hpp"
#include "libed2k/alert_types.hpp"
#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

namespace libed2k
{
    server_fingerprint::server_fingerprint(const std::string& nm, const std::string& hs, int p):
            name(nm), host(hs), port(p)
    {}

    server_connection_parameters::server_connection_parameters() :
        host(""), port(0),
        operations_timeout(pos_infin),
        keep_alive_timeout(pos_infin),
        reconnect_timeout(pos_infin),
        announce_timeout(pos_infin),
        announce_items_per_call_limit(50)
    {}

    server_connection_parameters::server_connection_parameters(const std::string& n, const std::string& h, int p,
                    int operations_t, int kpl_t, int reconnect_t, int announce_t, size_t ann_items_limit) :
                    name(n), host(h), port(p),
                    operations_timeout(operations_t>0?seconds(operations_t):pos_infin),
                    keep_alive_timeout(kpl_t>0?seconds(kpl_t):pos_infin),
                    reconnect_timeout(reconnect_t>0?seconds(reconnect_t):pos_infin),
                    announce_timeout(announce_t>0?seconds(announce_t):pos_infin),
                    announce_items_per_call_limit(ann_items_limit)
    {}

    void server_connection_parameters::set_operations_timeout(int timeout){
        operations_timeout = timeout>0?seconds(timeout):pos_infin;
    }

    void server_connection_parameters::set_keep_alive_timeout(int timeout){
        keep_alive_timeout = timeout>0?seconds(timeout):pos_infin;
    }

    void server_connection_parameters::set_reconnect_timeout(int timeout){
        reconnect_timeout = timeout>0?seconds(timeout):pos_infin;
    }

    void server_connection_parameters::set_announce_timeout(int timeout){
        announce_timeout = timeout>0?seconds(timeout):pos_infin;
    }

    server_connection::server_connection(aux::session_impl& ses):
        m_client_id(0),
        m_name_lookup(ses.m_io_service),
        m_ses(ses),
        m_tcp_flags(0),
        m_aux_port(0),
        m_socket(ses.m_io_service),
        current_operation(scs_stop),
        last_action_time(time_now()),
        announced_transfers_count(-1),
        last_close_result(errors::no_error)
    {
    }

    server_connection::~server_connection()
    {
        stop(boost::asio::error::operation_aborted);
    }

    void server_connection::start(const server_connection_parameters& p)
    {
        stop(boost::asio::error::operation_aborted);
        LIBED2K_ASSERT(current_operation == scs_stop);
        LIBED2K_ASSERT(p.port>0 && !p.host.empty());
        params = p;
        current_operation = scs_resolve;
        last_action_time = time_now();

        tcp::resolver::query q(params.host, boost::lexical_cast<std::string>(params.port));

        m_name_lookup.async_resolve(
            q, boost::bind(&server_connection::on_name_lookup, self(), _1, _2));
    }

    void server_connection::stop(const error_code& ec)
    {
        if (current_operation == scs_stop)
            return;

        DBG("server connection: disconnected");
        current_operation = scs_stop;
        last_action_time  = time_now();
        m_write_order.clear();  // remove all incoming messages
        m_socket.close();
        m_name_lookup.cancel();

        m_client_id = 0;
        m_tcp_flags = 0;
        m_aux_port  = 0;
        announced_transfers_count = 0;

        for (aux::session_impl_base::transfer_map::iterator i = m_ses.m_transfers.begin(); i != m_ses.m_transfers.end(); ++i)
        {
            transfer& t = *i->second;
            t.set_announced(false);
        }

        last_close_result = ec;
        m_ses.m_alerts.post_alert_should(server_connection_closed(params.name, params.host, params.port, ec));
    }

    void server_connection::post_search_request(search_request& ro)
    {
        search_request_block srb(ro);
        do_write(srb);
    }

    void server_connection::post_search_more_result_request()
    {
        search_more_result smr;
        do_write(smr);
    }

    void server_connection::post_sources_request(const md4_hash& hFile, boost::uint64_t nSize)
    {
        DBG("server_connection::post_sources_request(" << hFile.toString() << ", " << nSize << ")");
        get_file_sources gfs;
        gfs.m_hFile = hFile;
        gfs.m_file_size.nQuadPart = nSize;
        do_write(gfs);
    }

    void server_connection::post_announce(shared_files_list& offer_list)
    {
        DBG("server_connection::post_announce: " << offer_list.m_collection.size());
        do_write(offer_list);
    }

    void server_connection::post_callback_request(boost::uint32_t client_id)
    {
        DBG("server_connection::post_callback_request: " << client_id);
        callback_request_out cbo = {client_id};
        do_write(cbo);
    }

    void server_connection::second_tick(int tick_interval_ms)
    {

        ptime now = time_now();
        time_duration d = now - last_action_time;

        switch(current_operation)
        {
        case scs_stop:
            if (last_close_result &&
                    last_close_result != boost::asio::error::operation_aborted &&
                    d >= params.reconnect_timeout)
            {
                start(params);  // reconnect when last error status != aborted and timeout
            }
            break;
        case scs_resolve:
        case scs_connection:
        case scs_handshake:
            if (d >= params.operations_timeout)
            {
                stop(boost::asio::error::timed_out);
            }
            break;
        case scs_start:
            if (params.announce() && d >= params.announce_timeout)
            {
#ifdef LIBED2K_IS74
                if (announced_transfers_count != m_ses.m_transfers.size() + 1)
#else
                if (announced_transfers_count != m_ses.m_transfers.size())
#endif
                {
                    // unshared transfers exist
                    shared_files_list offer_list;

                    for (aux::session_impl_base::transfer_map::const_iterator i = m_ses.m_transfers.begin(); i != m_ses.m_transfers.end(); ++i)
                    {
                        // we send no more m_max_announces_per_call elements in one packet
                        if (offer_list.m_collection.size() >= params.announce_items_per_call_limit)
                        {
                            break;
                        }

                        transfer& t = *i->second;

                        // add transfer to announce list when it has one piece at least and it is not announced yet
                        if (!t.is_announced())
                        {
                            shared_file_entry se = t.get_announce(); // return empty entry on checking transfers and when num_have = 0

                            if (!se.is_empty())
                            {
                                offer_list.add(se);
                                t.set_announced(true); // mark transfer as announced
                                ++announced_transfers_count;
                            }
                        }
                    }

                    // generate announce for user as transfer when all transfers were announced but user wasn't
#ifdef LIBED2K_IS74
                    if (offer_list.m_collection.size() < params.announce_items_per_call_limit)
                    {
                        DBG("all transfer probably ware announced - announce user with correct size");
                        __file_size total_size;
                        total_size.nQuadPart = 0;

                        for (aux::session_impl_base::transfer_map::const_iterator i = m_ses.m_transfers.begin(); i != m_ses.m_transfers.end(); ++i)
                        {
                            transfer& t = *i->second;
                            total_size.nQuadPart += t.size();
                        }

                        shared_file_entry se;
                        se.m_hFile = m_ses.settings().user_agent;

                        if (tcp_flags() & SRV_TCPFLG_COMPRESSION)
                        {
                            // publishing an incomplete file
                            se.m_network_point.m_nIP    = 0xFBFBFBFB;
                            se.m_network_point.m_nPort  = 0xFBFB;
                        }
                        else
                        {
                            se.m_network_point.m_nIP     = client_id();
                            se.m_network_point.m_nPort   = m_ses.settings().listen_port;
                        }

                        // file name is user name with special mark
                        se.m_list.add_tag(make_string_tag(std::string("+++USERNICK+++ ") + m_ses.m_settings.client_name, FT_FILENAME, true));
                        se.m_list.add_tag(make_typed_tag(client_id(), FT_FILESIZE, true));

                        // write users size
                        if (tcp_flags() & SRV_TCPFLG_NEWTAGS)
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
                        post_announce(offer_list);
                    }
#endif
                    if (offer_list.m_size > 0)
                    {
                        DBG("session_impl::announce: " << offer_list.m_size);
                        post_announce(offer_list);
                    }

                    d = now - last_action_time; // update current idle duration for
                }
            }

            if (d >= params.keep_alive_timeout)
            {
                server_get_list sgl;
                do_write(sgl);
            }
            break;
        default:
            LIBED2K_ASSERT(false);
            break;
        }
    }

    void server_connection::on_name_lookup(
        const error_code& error, tcp::resolver::iterator i)
    {
        if (current_operation != scs_resolve)
            return;

        if (error || i == tcp::resolver::iterator())
        {
            ERR("server name: " << params.name << " host: " << params.host
                << ", resolve failed: " << error);
            stop(error);
            return;
        }

        DBG("server name resolved: " << libed2k::print_endpoint(m_target));
        current_operation = scs_connection;
        last_action_time = time_now();
        m_target = *i;
        m_ses.m_alerts.post_alert_should(server_name_resolved_alert(params.name, params.host, params.port, libed2k::print_endpoint(m_target)));
        m_socket.async_connect(m_target, boost::bind(&server_connection::on_connection_complete, self(), _1));
    }

    // private callback methods
    void server_connection::on_connection_complete(error_code const& error)
    {
        if (scs_connection != scs_connection)
            return;

        if (error)
        {
            ERR("connection to: " << libed2k::print_endpoint(m_target)
                << ", failed: " << error);
            stop(error);
            return;
        }

        DBG("connect to server:" << m_target << ", successfully");

        const session_settings& settings = m_ses.settings();

        cs_login_request    login;
        //!< generate initial packet to server
        boost::uint32_t nVersion = 0x3c;
        boost::uint32_t nCapability = CAPABLE_ZLIB | CAPABLE_AUXPORT | CAPABLE_NEWTAGS | CAPABLE_UNICODE | CAPABLE_LARGEFILES;
        boost::uint32_t nClientVersion  = (LIBED2K_VERSION_MAJOR << 24) | (LIBED2K_VERSION_MINOR << 17) | (LIBED2K_VERSION_TINY << 10) | (1 << 7);

        login.m_hClient                 = settings.user_agent;
        login.m_network_point.m_nIP     = 0;
        login.m_network_point.m_nPort   = settings.listen_port;

        login.m_list.add_tag(make_string_tag(std::string(settings.client_name), CT_NAME, true));
        login.m_list.add_tag(make_typed_tag(nVersion, CT_VERSION, true));
        login.m_list.add_tag(make_typed_tag(nCapability, CT_SERVER_FLAGS, true));
        login.m_list.add_tag(make_typed_tag(nClientVersion, CT_EMULE_VERSION, true));
        login.m_list.dump();

        do_read();

        current_operation = scs_handshake;
        last_action_time  = time_now();
        do_write(login);      // write login message
    }

    void server_connection::on_found_peers(const found_file_sources& sources)
    {
        APP("found peers for hash: " << sources.m_hFile);
        boost::shared_ptr<transfer> t = m_ses.find_transfer(sources.m_hFile).lock();

        if (!t) return;

        for (std::vector<net_identifier>::const_iterator i =
                 sources.m_sources.m_collection.begin();
             i != sources.m_sources.m_collection.end(); ++i)
        {
            tcp::endpoint peer(
                ip::address::from_string(int2ipstr(i->m_nIP)), i->m_nPort);

            if (isLowId(i->m_nIP) && !isLowId(m_client_id))
            {
                // peer LowID and we is not LowID - send callback request
                if (m_ses.register_callback(i->m_nIP, sources.m_hFile))
                    post_callback_request(i->m_nIP);
            }
            else
            {
                APP("found HiID peer: " << peer);
                t->add_peer(peer, peer_info::tracker);
            }
        }
    }

    void server_connection::handle_write(const error_code& error, size_t nSize)
    {
        CHECK_ABORTED()
        if (!error)
        {
            m_write_order.pop_front();

            if (!m_write_order.empty())
            {
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
            stop(error);
        }
    }

    void server_connection::do_read()
    {
        boost::asio::async_read(m_socket,
                       boost::asio::buffer(&m_in_header, header_size),
                       boost::bind(&server_connection::handle_read_header,
                               self(),
                               boost::asio::placeholders::error,
                               boost::asio::placeholders::bytes_transferred));
    }

    void server_connection::handle_read_header(const error_code& error, size_t nSize)
    {
        CHECK_ABORTED()
        error_code ec = error;

        if (!ec)
        {
            ec = m_in_header.check_packet();
        }

        if (!ec)
        {
            size_t nSize = m_in_header.service_size();

            switch(m_in_header.m_protocol)
            {
                case OP_EDONKEYPROT:
                case OP_EMULEPROT:
                {
                    m_in_container.resize(nSize);

                    if (nSize == 0)
                    {
                        handle_read_packet(boost::system::error_code(), 0); // all data was already read - execute callback
                    }
                    else
                    {
                        boost::asio::async_read(m_socket, boost::asio::buffer(&m_in_container[0], nSize),
                            boost::bind(&server_connection::handle_read_packet, self(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
                    }

                    break;
                }
                case OP_PACKEDPROT:
                {
                    if (nSize > 0){
                        DBG("gzip container - read it");
                        m_in_gzip_container.resize(nSize);
                        boost::asio::async_read(m_socket, boost::asio::buffer(&m_in_gzip_container[0], nSize),
                            boost::bind(&server_connection::handle_read_packet, self(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
                    }
                    else
                    {
                        DBG("packed packet is empty - ignore it");
                        // zero size packed packet - simply ignore it
                        do_read();
                    }
                    break;
                }
                default:
                    assert(false);  // it is impossible since check packet
                    break;
            }
        }
        else
        {
            stop(ec);
        }
    }

    void server_connection::handle_read_packet(const error_code& error, size_t nSize)
    {
        CHECK_ABORTED();

        typedef boost::iostreams::basic_array_source<char> Device;

        if (!error)
        {
            //DBG("server_connection::handle_read_packet(" << error.message() << ", " << nSize << ", " << packetToString(m_in_header.m_type));

            // gzip decompressor disabled
            if (m_in_header.m_protocol == OP_PACKEDPROT)
            {
                DBG("packed packet reseived");
                // unzip data
                m_in_container.resize(m_in_gzip_container.size() * 10 + 300);
                uLongf nSize = m_in_container.size();
                int nRet = uncompress((Bytef*)&m_in_container[0], &nSize, (const Bytef*)&m_in_gzip_container[0], m_in_gzip_container.size());

                if (nRet != Z_OK)
                {
                    DBG("Unzip error: ");
                    //unpack error - pass packet
                    do_read();
                    return;
                }

                m_in_container.resize(nSize);
            }


            boost::iostreams::stream_buffer<Device> buffer(&m_in_container[0], m_in_container.size());
            std::istream in_array_stream(&buffer);
            archive::ed2k_iarchive ia(in_array_stream);

            try
            {
                // dispatch message
                switch (m_in_header.m_type)
                {
                    case OP_REJECT:
                        DBG("ignore op_reject");
                        break;
                    case OP_DISCONNECT:
                        DBG("ignore op_disconnect");
                        break;
                    case OP_SERVERMESSAGE:
                    {
                        server_message smsg;
                        ia >> smsg;
                        m_ses.m_alerts.post_alert_should(server_message_alert(params.name, params.host, params.port, smsg.m_strMessage));
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
                        m_ses.m_alerts.post_alert_should(server_status_alert(params.name, params.host, params.port, sss.m_nFilesCount, sss.m_nUserCount));
                        break;
                    }
                    case OP_USERS_LIST:
                        DBG("ignore");
                        break;
                    case OP_IDCHANGE:
                    {
                        current_operation = scs_start;
                        id_change idc;
                        ia >> idc;

                        m_client_id = idc.m_client_id;
                        m_tcp_flags = idc.m_tcp_flags;
                        m_aux_port  = idc.m_aux_port;
                        DBG("handshake finished. server connection opened {" << idc << "}" << (isLowId(idc.m_client_id)?"LowID":"HighID"));
                        m_ses.m_alerts.post_alert_should(server_connection_initialized_alert(params.name, params.host, params.port, m_client_id, m_tcp_flags, m_aux_port));
                        break;
                    }
                    case OP_SERVERIDENT:
                    {
                        server_info_entry se;
                        ia >> se;
                        se.dump();
                        m_hServer = se.m_hServer;
                        m_ses.m_alerts.post_alert_should(server_identity_alert(params.name, params.host, params.port, se.m_hServer, se.m_network_point,
                                se.m_list.getStringTagByNameId(ST_SERVERNAME),
                                se.m_list.getStringTagByNameId(ST_DESCRIPTION)));
                        break;
                    }
                    case OP_FOUNDSOURCES:
                    {
                        found_file_sources fs;
                        ia >> fs;
                        fs.dump();
                        on_found_peers(fs);
                        break;
                    }
                    case OP_SEARCHRESULT:
                    {
                        search_result sfl;
                        ia >> sfl;

                        m_ses.m_alerts.post_alert_should(
                                shared_files_alert(net_identifier(address2int(m_target.address()), m_target.port()), m_hServer,
                                        sfl.m_files, (sfl.m_more_results_avaliable != 0)));
                        break;
                    }
                    case OP_CALLBACKREQUESTED:
                    {
                        callback_request_in cb;
                        ia >> cb;
                        // connect to requested client
                        error_code ec;
                        m_ses.add_peer_connection(cb.m_network_point, ec);
                        if (ec)
                        {
                            ERR("Unable to connect to {" << cb.m_network_point.m_nIP << ":" << cb.m_network_point.m_nPort << "} by callback request");
                        }
                        break;
                    }
                    case OP_CALLBACK_FAIL:
                        DBG("callback request failed - cleanup callbacks? ");
                        break;
                    default:
                        ERR("ignore unhandled packet: " << m_in_header.m_type);
                        break;
                }

                m_in_gzip_container.clear();
                m_in_container.clear();

                do_read();

            }
            catch(libed2k_exception&)
            {
                ERR("packet parse error");
                stop(errors::decode_packet_error);
            }
        }
        else
        {
            stop(error);
        }
    }
}
