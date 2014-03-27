#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread.hpp>
#include <boost/asio/error.hpp>

#include <libed2k/bencode.hpp>

#include "libed2k/log.hpp"
#include "libed2k/session.hpp"
#include "libed2k/session_settings.hpp"
#include "libed2k/util.hpp"
#include "libed2k/alert_types.hpp"
#include "libed2k/file.hpp"
#include "libed2k/search.hpp"
#include "libed2k/peer_connection_handle.hpp"
#include "libed2k/transfer_handle.hpp"
#include "libed2k/io_service.hpp"
#include "libed2k/server_connection.hpp"

using namespace libed2k;

enum KAD_CMD
{
    kad_empty
};

KAD_CMD extract_cmd(const std::string& strCMD, std::string& strArg)
{
    if (strCMD.empty())
    {
        return kad_empty;
    }

    std::string::size_type nPos = strCMD.find_first_of(":");
    std::string strCommand;

    if (nPos == std::string::npos)
    {
       strCommand = strCMD;
       strArg.clear();
    }
    else
    {
        strCommand = strCMD.substr(0, nPos);
        strArg = strCMD.substr(nPos+1);
    }

    return kad_empty;
}

libed2k::shared_files_list vSF;
boost::mutex m_sf_mutex;

void alerts_reader(const boost::system::error_code& ec, boost::asio::deadline_timer* pt, libed2k::session* ps)
{
    if (ec == boost::asio::error::operation_aborted)
    {
        return;
    }

    std::auto_ptr<alert> a = ps->pop_alert();

    while(a.get())
    {
        if (dynamic_cast<server_connection_initialized_alert*>(a.get()))
        {
            server_connection_initialized_alert* p = dynamic_cast<server_connection_initialized_alert*>(a.get());
            DBG("ALERT:  " << "server initalized: cid: " << p->client_id);
        }
        else if (dynamic_cast<server_name_resolved_alert*>(a.get()))
        {
            DBG("ALERT: server name was resolved: " << dynamic_cast<server_name_resolved_alert*>(a.get())->endpoint);
        }
        else if (dynamic_cast<server_status_alert*>(a.get()))
        {
            server_status_alert* p = dynamic_cast<server_status_alert*>(a.get());
            DBG("ALERT: server status: files count: " << p->files_count << " users count " << p->users_count);
        }
        else if (dynamic_cast<server_message_alert*>(a.get()))
        {
            server_message_alert* p = dynamic_cast<server_message_alert*>(a.get());
            DBG("ALERT: " << "msg: " << p->server_message);
        }
        else if (dynamic_cast<server_identity_alert*>(a.get()))
        {
            server_identity_alert* p = dynamic_cast<server_identity_alert*>(a.get());
            DBG("ALERT: server_identity_alert: " << p->server_hash << " name:  " << p->server_name << " descr: " << p->server_descr);
        }
        else if (shared_files_alert* p = dynamic_cast<shared_files_alert*>(a.get()))
        {
            boost::mutex::scoped_lock l(m_sf_mutex);
            DBG("ALERT: RESULT: " << p->m_files.m_collection.size());
            vSF.clear();
            vSF = p->m_files;

            boost::uint64_t nSize = 0;

            for (size_t n = 0; n < vSF.m_size; ++n)
            {
                boost::shared_ptr<base_tag> low = vSF.m_collection[n].m_list.getTagByNameId(libed2k::FT_FILESIZE);
                boost::shared_ptr<base_tag> hi = vSF.m_collection[n].m_list.getTagByNameId(libed2k::FT_FILESIZE_HI);
                boost::shared_ptr<base_tag> src = vSF.m_collection[n].m_list.getTagByNameId(libed2k::FT_SOURCES);

                if (low.get())
                {
                    nSize = low->asInt();
                }

                if (hi.get())
                {
                    nSize += hi->asInt() << 32;
                }

                DBG("ALERT: indx:" << n << " hash: " << vSF.m_collection[n].m_hFile.toString()
                    << " name: " << libed2k::convert_to_native(vSF.m_collection[n].m_list.getStringTagByNameId(libed2k::FT_FILENAME))
                    << " size: " << nSize
                    << " src: " << src->asInt());
            }
        }
        else if(dynamic_cast<peer_message_alert*>(a.get()))
        {
            peer_message_alert* p = dynamic_cast<peer_message_alert*>(a.get());
            DBG("ALERT: MSG: ADDR: " << int2ipstr(p->m_np.m_nIP) << " MSG " << p->m_strMessage);
        }
        else if (peer_disconnected_alert* p = dynamic_cast<peer_disconnected_alert*>(a.get()))
        {
            DBG("ALERT: peer disconnected: " << libed2k::int2ipstr(p->m_np.m_nIP));
        }
        else if (peer_captcha_request_alert* p = dynamic_cast<peer_captcha_request_alert*>(a.get()))
        {
            DBG("ALERT: captcha request ");
            FILE* fp = fopen("./captcha.bmp", "wb");

            if (fp)
            {
                fwrite(&p->m_captcha[0], 1, p->m_captcha.size(), fp);
                fclose(fp);
            }

        }
        else if (peer_captcha_result_alert* p = dynamic_cast<peer_captcha_result_alert*>(a.get()))
        {
            DBG("ALERT: captcha result " << p->m_nResult);
        }
        else if (peer_connected_alert* p = dynamic_cast<peer_connected_alert*>(a.get()))
        {
            DBG("ALERT: peer connected: " << int2ipstr(p->m_np.m_nIP) << " status: " << p->m_active);
        }
        else if (shared_files_access_denied* p = dynamic_cast<shared_files_access_denied*>(a.get()))
        {
            DBG("ALERT: peer denied access to shared files: " << int2ipstr(p->m_np.m_nIP));
        }
        else if (shared_directories_alert* p = dynamic_cast<shared_directories_alert*>(a.get()))
        {
            DBG("peer shared directories: " << int2ipstr(p->m_np.m_nIP) << " count: " << p->m_dirs.size());

            for (size_t n = 0; n < p->m_dirs.size(); ++n)
            {
                DBG("ALERT: DIR: " << p->m_dirs[n]);
            }
        }
        else if (dynamic_cast<save_resume_data_alert*>(a.get()))
        {
            DBG("ALERT: save_resume_data_alert");
        }
        else if (dynamic_cast<save_resume_data_failed_alert*>(a.get()))
        {
            DBG("ALERT: save_resume_data_failed_alert");
        }
        else if(transfer_params_alert* p = dynamic_cast<transfer_params_alert*>(a.get()))
        {
        	if (!p->m_ec)
        	{
        		DBG("ALERT: transfer_params_alert, add transfer for: " << p->m_atp.file_path);
        		ps->add_transfer(p->m_atp);
        	}
        }
        else
        {
            DBG("ALERT: Unknown alert: " << a.get()->message());
        }

        a = ps->pop_alert();
    }

    pt->expires_at(pt->expires_at() + boost::posix_time::seconds(3));
    pt->async_wait(boost::bind(&alerts_reader, boost::asio::placeholders::error, pt, ps));
}

int main(int argc, char* argv[])
{
    LOGGER_INIT(LOG_ALL)

    if (argc < 4)
    {
        ERR("Set server host, port and incoming directory");
        return (1);
    }

    DBG("Server: " << argv[1] << " port: " << argv[2]);

    // immediately convert to utf8
    std::string strIncomingDirectory = libed2k::convert_from_native(argv[3]);

    libed2k::fingerprint print;
    libed2k::session_settings settings;
    settings.m_known_file = "./known.met";
    settings.listen_port = 4668;
    //settings.server_
    libed2k::session ses(print, "0.0.0.0", settings);
    ses.set_alert_mask(alert::all_categories);

    libed2k::server_connection_parameters scp("New server", argv[1], atoi(argv[2]), 20, 20, 10, 10, 10);

    libed2k::io_service io;
    boost::asio::deadline_timer alerts_timer(io, boost::posix_time::seconds(3));
    boost::asio::deadline_timer fs_timer(io, boost::posix_time::minutes(3));
    alerts_timer.async_wait(boost::bind(alerts_reader, boost::asio::placeholders::error, &alerts_timer, &ses));
    boost::thread t(boost::bind(&libed2k::io_service::run, &io));

    libed2k::search_request order = libed2k::generateSearchRequest(0,0,0,0, "", "", "", 0, 0, "db2");

    std::cout << "---- libed2k_client started\n"
              << "---- press q to exit\n"
              << "---- press something other for process alerts " << std::endl;


    std::string strAlex = "109.191.73.222";
    std::string strDore = "192.168.161.54";
    std::string strDiman = "88.206.52.81";
    ip::address a(ip::address::from_string(strDore.c_str()));
    int nPort = 4667;

    DBG("addr: "<< int2ipstr(address2int(a)));
    std::string strUser;
    libed2k::peer_connection_handle pch;

    net_identifier ni(address2int(a), nPort);

    std::string strArg;
    std::deque<std::string> vpaths;
    ses.server_connect(scp);

    while ((std::cin >> strUser))
    {
        DBG("process: " << strUser);

        if (strUser == "quit")
        {
            break;
        }

        switch(extract_cmd(strUser, strArg))
        {
            default:
                break;
        }
    }

    // cancel background operations and wait
    io.post(boost::bind(&boost::asio::deadline_timer::cancel, &alerts_timer));
    io.post(boost::bind(&boost::asio::deadline_timer::cancel, &fs_timer));
    t.join();

    return 0;
}



