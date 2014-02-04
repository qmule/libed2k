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

/**
 md4_hash::dump D4CF11BC699F0850210A92BEE8DFCD12
23:57.11 4860[dbg] net_identifier::dump(IP=1768108042 port=4662)
23:57.11 4861[dbg] size type is: 4
23:57.11 4862[dbg] count: 7
23:57.11 4863[dbg] base_tag::dump
23:57.11 4864[dbg] type: TAGTYPE_STRING
23:57.11 4865[dbg] name:  tag id: FT_FILENAME
23:57.11 4866[dbg] VALUEСоблазны в больнице.XXX
23:57.11 4867[dbg] base_tag::dump
23:57.11 4868[dbg] type: TAGTYPE_UINT32
23:57.11 4869[dbg] name:  tag id: FT_FILESIZE
23:57.11 4870[dbg] VALUE: 732807168
23:57.11 4871[dbg] base_tag::dump
23:57.11 4872[dbg] type: TAGTYPE_UINT8
23:57.11 4873[dbg] name:  tag id: FT_SOURCES
23:57.11 4874[dbg] VALUE: 43
23:57.11 4875[dbg] base_tag::dump
23:57.11 4876[dbg] type: TAGTYPE_UINT8
23:57.11 4877[dbg] name:  tag id: FT_COMPLETE_SOURCES
23:57.11 4878[dbg] VALUE: 42
23:57.11 4879[dbg] base_tag::dump
23:57.11 4880[dbg] type: TAGTYPE_UINT16
23:57.11 4881[dbg] name:  tag id: FT_MEDIA_BITRATE
23:57.11 4882[dbg] VALUE: 982
23:57.11 4883[dbg] base_tag::dump
23:57.11 4884[dbg] type: TAGTYPE_STR4
23:57.11 4885[dbg] name:  tag id: FT_MEDIA_CODEC
23:57.11 4886[dbg] VALUExvid
23:57.11 4887[dbg] base_tag::dump
23:57.11 4888[dbg] type: TAGTYPE_UINT16
23:57.11 4889[dbg] name:  tag id: FT_MEDIA_LENGTH
23:57.11 4890[dbg] VALUE: 5904
23:57.11 4891[dbg] search_file_entry::dump
23:57.11 4892[dbg] md4_hash::dump 6FE930EE2BB8B4E5B960811346331A43
23:57.11 4893[dbg] net_identifier::dump(IP=576732682 port=4666)
23:57.11 4894[dbg] size type is: 4
23:57.11 4895[dbg] count: 7
23:57.11 4896[dbg] base_tag::dump
23:57.11 4897[dbg] type: TAGTYPE_STRING
23:57.11 4898[dbg] name:  tag id: FT_FILENAME
23:57.11 4899[dbg] VALUEXXX Видео Bangbros. Nasty Naom.wmv
23:57.11 4900[dbg] base_tag::dump
23:57.11 4901[dbg] type: TAGTYPE_UINT32
23:57.11 4902[dbg] name:  tag id: FT_FILESIZE
23:57.11 4903[dbg] VALUE: 6779182
23:57.11 4904[dbg] base_tag::dump
23:57.11 4905[dbg] type: TAGTYPE_UINT8
23:57.11 4906[dbg] name:  tag id: FT_SOURCES
23:57.11 4907[dbg] VALUE: 2
23:57.11 4908[dbg] base_tag::dump
23:57.11 4909[dbg] type: TAGTYPE_UINT8
23:57.11 4910[dbg] name:  tag id: FT_COMPLETE_SOURCES
23:57.11 4911[dbg] VALUE: 2
23:57.11 4912[dbg] base_tag::dump
23:57.11 4913[dbg] type: TAGTYPE_UINT16
23:57.11 4914[dbg] name:  tag id: FT_MEDIA_BITRATE
23:57.11 4915[dbg] VALUE: 1017
23:57.11 4916[dbg] base_tag::dump
23:57.11 4917[dbg] type: TAGTYPE_STR4
23:57.11 4918[dbg] name:  tag id: FT_MEDIA_CODEC
23:57.11 4919[dbg] VALUEwmv2
23:57.11 4920[dbg] base_tag::dump
23:57.11 4921[dbg] type: TAGTYPE_UINT8
23:57.11 4922[dbg] name:  tag id: FT_MEDIA_LENGTH
23:57.11 4923[dbg] VALUE: 54
23:57.11 4924[dbg] Results count: 52


 */

enum CONN_CMD
{
    cc_search,
    cc_simplesearch,
    cc_download,
    cc_download_all,
    cc_save_fast_resume,
    cc_restore,
    cc_share,
    cc_unshare,
    cc_sharef,
    cc_unsharef,
    cc_remove,
    cc_dump,
    cc_connect,
    cc_disconnect,
    cc_listen,
    cc_empty,
    cc_tr,
    cc_search_test
};

CONN_CMD extract_cmd(const std::string& strCMD, std::string& strArg)
{

    if (strCMD.empty())
    {
        return cc_empty;
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

    if (strCommand == "search")
    {
        return cc_search;
    }
    if (strCommand == "simplesearch")
    {
        return cc_simplesearch;
    }
    else if (strCommand == "load")
    {
        return cc_download;
    }
    else if (strCommand == "loadall")
    {
        return cc_download_all;
    }
    else if (strCommand == "save")
    {
        return cc_save_fast_resume;
    }
    else if (strCommand == "restore")
    {
        return cc_restore;
    }
    else if (strCommand == "share")
    {
        return cc_share;
    }
    else if (strCommand == "remove")
    {
        return cc_remove;
    }
    else if (strCommand == "dump")
    {
        return cc_dump;
    }
    else if (strCommand == "unshare")
    {
        return cc_unshare;
    }
    else if (strCommand == "sharef")
    {
        return cc_sharef;
    }
    else if (strCommand == "unsharef")
    {
        return cc_unsharef;
    }
    else if (strCommand == "connect")
    {
        return cc_connect;
    }
    else if (strCommand == "disconnect")
    {
        return cc_disconnect;
    }
    else if (strCommand == "listen")
    {
        return cc_listen;
    }
    else if (strCommand == "tr")
    {
        return cc_tr;
    }
    else if (strCommand == "sst")
    {
        return cc_search_test;
    }

    return cc_empty;
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
            DBG("ALERT:  " << "server initalized: cid: " << p->m_nClientId);
        }
        else if (dynamic_cast<server_name_resolved_alert*>(a.get()))
        {
            DBG("ALERT: server name was resolved: " << dynamic_cast<server_name_resolved_alert*>(a.get())->m_strServer);
        }
        else if (dynamic_cast<server_status_alert*>(a.get()))
        {
            server_status_alert* p = dynamic_cast<server_status_alert*>(a.get());
            DBG("ALERT: server status: files count: " << p->m_nFilesCount << " users count " << p->m_nUsersCount);
        }
        else if (dynamic_cast<server_message_alert*>(a.get()))
        {
            server_message_alert* p = dynamic_cast<server_message_alert*>(a.get());
            DBG("ALERT: " << "msg: " << p->m_strMessage);
        }
        else if (dynamic_cast<server_identity_alert*>(a.get()))
        {
            server_identity_alert* p = dynamic_cast<server_identity_alert*>(a.get());
            DBG("ALERT: server_identity_alert: " << p->m_hServer << " name:  " << p->m_strName << " descr: " << p->m_strDescr);
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

void save_fast_resume(const boost::system::error_code& ec, boost::asio::deadline_timer* pt, libed2k::session* ps)
{
    if (ec == boost::asio::error::operation_aborted)
    {
        return;
    }

    // actually we save nothing - only for test
    std::vector<libed2k::transfer_handle> v = ps->get_transfers();
    int num_resume_data = 0;

    for (std::vector<libed2k::transfer_handle>::iterator i = v.begin(); i != v.end(); ++i)
    {
       libed2k::transfer_handle h = *i;

       if (!h.is_valid()) continue;

       DBG("save transfer " << h.hash().toString());

       try
       {

         if (h.status().state == libed2k::transfer_status::checking_files ||
             h.status().state == libed2k::transfer_status::queued_for_checking) continue;
         h.save_resume_data();
         ++num_resume_data;
       }
       catch(libed2k::libed2k_exception& e)
       {
           ERR("save error: " << e.what());
       }
    }

    DBG("total saved: "<< num_resume_data);

    pt->expires_at(pt->expires_at() + boost::posix_time::minutes(3));
    pt->async_wait(boost::bind(&save_fast_resume, boost::asio::placeholders::error, pt, ps));
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

    libed2k::server_connection_parameters scp(argv[1], argv[2], 2, 20, 10, -1, 0);

    libed2k::io_service io;
    boost::asio::deadline_timer alerts_timer(io, boost::posix_time::seconds(3));
    boost::asio::deadline_timer fs_timer(io, boost::posix_time::minutes(3));
    alerts_timer.async_wait(boost::bind(alerts_reader, boost::asio::placeholders::error, &alerts_timer, &ses));
    fs_timer.async_wait(boost::bind(save_fast_resume, boost::asio::placeholders::error, &fs_timer, &ses));
    boost::thread t(boost::bind(&libed2k::io_service::run, &io));


    /*
    sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_NOT));
    sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_AND));
    sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_AND));
    sr.add_entry(libed2k::search_request_entry("dead"));
    sr.add_entry(libed2k::search_request_entry("walking"));
    sr.add_entry(libed2k::search_request_entry(FT_FILESIZE, ED2K_SEARCH_OP_GREATER, 300));
    sr.add_entry(libed2k::search_request_entry("HD"));
*/
    //sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_NOT));
    //sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_OR));
    //sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_AND));
    //sr.add_entry(libed2k::search_request_entry("dead"));
    //sr.add_entry(libed2k::search_request_entry("kkkkJKJ"));

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
            case cc_search:
            {
                // execute search
                DBG("Execute search request: " << strArg);
                search_request sr = libed2k::generateSearchRequest(1000000000,0,1,0, "", "", "", 0, 0, libed2k::convert_from_native(strArg));
                ses.post_search_request(sr);
                break;
            }
            case cc_search_test:
            {
                DBG("Execute search test");
                search_request sr;
                sr.push_back(search_request_entry(search_request_entry::SRE_NOT));
                sr.push_back(search_request_entry(search_request_entry::SRE_OR));
                sr.push_back(search_request_entry(search_request_entry::SRE_AND));
                sr.push_back(search_request_entry("a"));
                sr.push_back(search_request_entry("b"));
                sr.push_back(search_request_entry(search_request_entry::SRE_AND));
                sr.push_back(search_request_entry("c"));
                sr.push_back(search_request_entry("d"));
                sr.push_back(search_request_entry("+++"));
                ses.post_search_request(sr);
                break;
            }
            case cc_simplesearch:
            {
                DBG("Execute simple search request: " << strArg);
                search_request sr = libed2k::generateSearchRequest(0,0,0,0, "", "", "", 0, 0, strArg);
                ses.post_search_request(sr);
                break;    
            }
            case cc_download:
            {
                boost::mutex::scoped_lock l(m_sf_mutex);
                int nIndex = atoi(strArg.c_str());

                DBG("execute load for " << nIndex);

                if (vSF.m_collection.size() > static_cast<size_t>(nIndex))
                {
                    DBG("load for: " << vSF.m_collection[nIndex].m_hFile.toString());
                    libed2k::add_transfer_params params;
                    params.file_hash = vSF.m_collection[nIndex].m_hFile;
                    params.file_path = strIncomingDirectory;
                    params.file_path = libed2k::combine_path(params.file_path, vSF.m_collection[nIndex].m_list.getStringTagByNameId(libed2k::FT_FILENAME));
                    params.file_size = vSF.m_collection[nIndex].m_list.getTagByNameId(libed2k::FT_FILESIZE)->asInt();

                    if (vSF.m_collection[nIndex].m_list.getTagByNameId(libed2k::FT_FILESIZE_HI))
                    {
                        params.file_size += vSF.m_collection[nIndex].m_list.getTagByNameId(libed2k::FT_FILESIZE_HI)->asInt() << 32;
                    }

                    ses.add_transfer(params);
                }
                break;
            }
            case cc_download_all:
            {
                int nBorder = atoi(strArg.c_str());
                DBG("Load first " << nBorder);
                for (size_t nIndex = 0; nIndex < vSF.m_collection.size(); ++nIndex)
                {
                    if (nIndex >= static_cast<size_t>(nBorder))
                    {
                        break;
                    }

                    DBG("load for: " << nIndex << " " << vSF.m_collection[nIndex].m_hFile.toString());
                    libed2k::add_transfer_params params;
                    params.file_hash = vSF.m_collection[nIndex].m_hFile;
                    params.file_path = strIncomingDirectory;
                    params.file_path = libed2k::combine_path(params.file_path, vSF.m_collection[nIndex].m_list.getStringTagByNameId(libed2k::FT_FILENAME));
                    params.file_size = vSF.m_collection[nIndex].m_list.getTagByNameId(libed2k::FT_FILESIZE)->asInt();

                    if (vSF.m_collection[nIndex].m_list.getTagByNameId(libed2k::FT_FILESIZE_HI))
                    {
                        params.file_size += vSF.m_collection[nIndex].m_list.getTagByNameId(libed2k::FT_FILESIZE_HI)->asInt() << 32;
                    }

                    ses.add_transfer(params);
#ifndef WIN32
                    sleep(1);
#else
                    Sleep(500);
#endif

                }

                break;
            }
            case cc_remove:
            {
                std::vector<libed2k::transfer_handle> v = ses.get_transfers();
                for (size_t n = 0; n < v.size(); ++n)
                {
                    try
                    {
                        ses.remove_transfer(v[n], 0);
                    }
                    catch(libed2k::libed2k_exception&)
                    {}
                }
                break;
            }
            case cc_save_fast_resume:
            {
                DBG("Save fast resume data");
                vpaths.clear();
                std::vector<libed2k::transfer_handle> v = ses.get_transfers();
                int num_resume_data = 0;
                for (std::vector<libed2k::transfer_handle>::iterator i = v.begin(); i != v.end(); ++i)
                {
                    libed2k::transfer_handle h = *i;
                    if (!h.is_valid()) continue;

                    DBG("save for " << num_resume_data);

                    try
                    {

                      if (h.status().state == libed2k::transfer_status::checking_files ||
                          h.status().state == libed2k::transfer_status::queued_for_checking) continue;

                      DBG("call save");
                      h.save_resume_data();
                      ++num_resume_data;
                    }
                    catch(libed2k::libed2k_exception&)
                    {}
                }

                while (num_resume_data > 0)
                {
                    DBG("wait for alert");
                    libed2k::alert const* a = ses.wait_for_alert(libed2k::seconds(30));

                    if (a == 0)
                    {
                        DBG(" aborting with " << num_resume_data << " outstanding torrents to save resume data for");
                        break;
                    }

                    DBG("alert ready");

                    // Saving fastresume data can fail
                    libed2k::save_resume_data_failed_alert const* rda = dynamic_cast<libed2k::save_resume_data_failed_alert const*>(a);

                    if (rda)
                    {
                        DBG("save failed");
                        --num_resume_data;
                        ses.pop_alert();
                        try
                        {
                        // Remove torrent from session
                        if (rda->m_handle.is_valid()) ses.remove_transfer(rda->m_handle, 0);
                        }
                        catch(libed2k::libed2k_exception&)
                        {}
                        continue;
                    }

                    libed2k::save_resume_data_alert const* rd = dynamic_cast<libed2k::save_resume_data_alert const*>(a);

                    if (!rd)
                    {
                        ses.pop_alert();
                        continue;
                    }

                    DBG("Saving fast resume data was succesfull");
                    // write fast resume data
                    std::vector<char> vFastResumeData;
                    libed2k::bencode(std::back_inserter(vFastResumeData), *rd->resume_data);
                    DBG("save data size: " << vFastResumeData.size());

                    // Saving fast resume data was successful
                    --num_resume_data;

                    if (!rd->m_handle.is_valid()) continue;

                    libed2k::transfer_resume_data trd(rd->m_handle.hash(), rd->m_handle.save_path(), rd->m_handle.name(), rd->m_handle.size(), vFastResumeData);

                    // prepare storage filename
                    std::string strStorage = std::string("./") + rd->m_handle.hash().toString();
                    vpaths.push_back(strStorage);

                    std::ofstream fs(strStorage.c_str(), std::ios_base::out | std::ios_base::binary);

                    if (fs)
                    {
                        libed2k::archive::ed2k_oarchive oa(fs);
                        oa << trd;
                    }


                    ses.pop_alert();
                }
                break;
            }
            case cc_restore:
            {
                for (size_t n = 0; n < vpaths.size(); ++n)
                {
                    DBG("restore " << vpaths[n]);

                    std::ifstream ifs(vpaths[n].c_str(), std::ios_base::in | std::ios_base::binary);

                    if (ifs)
                    {
                        libed2k::transfer_resume_data trd;
                        libed2k::archive::ed2k_iarchive ia(ifs);
                        ia >> trd;

                        libed2k::add_transfer_params params;
                        params.seed_mode = false;
                        params.file_path= trd.m_filepath.m_collection;
                        params.file_size = trd.m_filesize;

                        if (trd.m_fast_resume_data.count() > 0)
                        {
                            params.resume_data = const_cast<std::vector<char>* >(&trd.m_fast_resume_data.getTagByNameId(libed2k::FT_FAST_RESUME_DATA)->asBlob());
                        }

                        params.file_hash = trd.m_hash;
                        ses.add_transfer(params);
                    }
                }
                break;
            }
            case cc_share:
            {
                DBG("share directory: " << strArg);
                libed2k::error_code ec;
                libed2k::directory dir(strArg, ec);
                if (!ec)
                {
                	while(!dir.done())
                	{
                		dir.next(ec);
                		ses.make_transfer_parameters(libed2k::combine_path(strArg, dir.file()));
                	}
                }
                else
                {
                	DBG(ec.message());
                }

                //ses.share_dir(strArg, strArg, v);
                break;
            }
            case cc_unshare:
            {
                DBG("unshare " << strArg);
                std::deque<std::string> v;
                //ses.unshare_dir(strArg, strArg, v);
                break;
            }
            case cc_sharef:
            {
                DBG("share file " << strArg);
                //ses.share_file(strArg);
                break;
            }
            case cc_unsharef:
            {
                DBG("unshare file " << strArg);
                //ses.unshare_file(strArg);
                break;
            }
            case cc_dump:
            {
                std::vector<libed2k::transfer_handle> v = ses.get_transfers();
                for (std::vector<libed2k::transfer_handle>::iterator i = v.begin(); i != v.end(); ++i)
                {
                    DBG("transfer: {" << i->hash().toString() << "}{"
                            << libed2k::convert_to_native(i->save_path()) << "}{"
                            << libed2k::convert_to_native(i->name()) << "}{"
                            << i->size() << "}{"
                            << libed2k::transfer_status2string(i->status()) << "}{A/S:" << ((i->is_announced())?"Y":"N") << "}");
                }
                break;
            }
            case cc_connect:
                ses.server_connect(scp);
                break;
            case cc_disconnect:
                ses.server_disconnect();
                break;
            case cc_listen:
                {
                    settings.listen_port = atoi(strArg.c_str());
                    if (ses.listen_on(settings.listen_port))
                    {
                        DBG("Ok, listen on " << strArg);
                    }
                    else
                    {
                        DBG("Unable to reset port");
                    }
                    break;
                }
            case cc_tr:
            {
                std::vector<libed2k::transfer_handle> vth = ses.get_transfers();
                for (size_t i = 0; i < vth.size(); ++i)
                {
                    DBG("TR: " << vth[i].hash().toString()
                            << " valid: " << (vth[i].is_valid()?"valid":"invalid")
                            << " urate: " << vth[i].status().upload_payload_rate
                            << " drate: " << vth[i].status().download_payload_rate);
                }
                break;
            }
            default:
                break;
        }

        if (!strUser.empty() && strUser.size() == 1)
        {
            switch(strUser.at(0))
            {
            case 'd':
                ses.server_disconnect();
                break;
            case 'c':
                ses.server_connect(scp);
                break;
            case 'f':
                {
                    if (pch.empty())
                    {
                        pch = ses.add_peer_connection(ni);
                    }

                    DBG("get shared files");
                    pch.get_shared_files();
                    break;
                }
            case 'm':
                {
                    if (pch.empty())
                    {
                        DBG("pch empty - create it");
                        pch = ses.add_peer_connection(ni);
                    }

                    DBG("pch send message");
                    pch.send_message("Hello it is peer connection handle");
                }
                break;
            case 's':
            {
                if (pch.empty())
                {
                    pch = ses.add_peer_connection(ni);
                }

                pch.get_shared_files();
            }
                break;
            case 'r':
            {
                if (pch.empty())
                {
                    pch = ses.add_peer_connection(ni);
                }

                DBG("get shared directories");

                pch.get_shared_directories();
                break;
            }
            case 'e':
            {
                if (pch.empty())
                {
                    pch = ses.add_peer_connection(ni);
                }

                DBG("get shared files");
                pch.get_shared_directory_files("/home/d95a1/sqllib/samples/cpp");
                break;
            }
            case 'i':
            {
                pch = ses.find_peer_connection(ni);

                if (pch.empty())
                {
                    DBG("peer connection not exists - add it");
                    pch = ses.add_peer_connection(ni);
                }
                break;
            }
            default:
                break;
            };
        }
    }

    // cancel background operations and wait
    io.post(boost::bind(&boost::asio::deadline_timer::cancel, &alerts_timer));
    io.post(boost::bind(&boost::asio::deadline_timer::cancel, &fs_timer));
    t.join();

    return 0;
}



