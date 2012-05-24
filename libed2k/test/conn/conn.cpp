#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "libed2k/log.hpp"
#include "libed2k/session.hpp"
#include "libed2k/session_settings.hpp"
#include "libed2k/alert_types.hpp"
#include "libed2k/util.hpp"
#include "libed2k/file.hpp"
#include "libed2k/search.hpp"

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

int main(int argc, char* argv[])
{
    LOGGER_INIT()

    if (argc < 3)
    {
        ERR("Set server host and port");
        return (1);
    }

    DBG("Server: " << argv[1] << " port: " << argv[2]);

    libed2k::fingerprint print;
    libed2k::session_settings settings;
    settings.server_keep_alive_timeout = 20;
    settings.server_reconnect_timeout = 30;
    settings.server_hostname = argv[1];
    settings.server_timeout = 25;
    //settings.server_
    libed2k::session ses(print, "0.0.0.0", settings);
    ses.set_alert_mask(alert::all_categories);

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
    libed2k::search_request order = libed2k::generateSearchRequest(0,0,0,0, "", "", "", 0, 0, "hpp"/*"'+++USERNICK+++' A B"*/);

    std::cout << "---- libed2k_client started\n"
              << "---- press q to exit\n"
              << "---- press something other for process alerts " << std::endl;

    std::string strUser;
    while ((std::cin >> strUser))
    {
        if (strUser == "quit")
        {
            break;
        }

        if (!strUser.empty() && strUser.size() == 1)
        {
            switch(strUser.at(0))
            {
            case 'd':
                ses.server_conn_stop();
                break;
            case 'c':
                ses.server_conn_start();
                break;
            case 'f':
                ses.post_search_request(order);
                break;
            case 'm':
                ses.post_message("192.168.161.32", 4664, "Hello aMule");
                break;
            default:
                break;
            };
        }

        if (strUser.size() > 1)
        {
            ses.post_message("192.168.161.32", 4664, strUser);
        }


        std::auto_ptr<alert> a = ses.pop_alert();

        while(a.get())
        {
            if (dynamic_cast<server_connection_initialized_alert*>(a.get()))
            {
                server_connection_initialized_alert* p = dynamic_cast<server_connection_initialized_alert*>(a.get());
                std::cout << "server initalized: cid: "
                        << p->m_nClientId
                        << std::endl;
                DBG("send search request");
                //ses.post_search_request(order);
            }
            else if (dynamic_cast<server_name_resolved_alert*>(a.get()))
            {
                DBG("server name was resolved: " << dynamic_cast<server_name_resolved_alert*>(a.get())->m_strServer);
            }
            else if (dynamic_cast<server_status_alert*>(a.get()))
            {
                server_status_alert* p = dynamic_cast<server_status_alert*>(a.get());
                DBG("server status: files count: " << p->m_nFilesCount << " users count " << p->m_nUsersCount);
            }
            else if (dynamic_cast<server_message_alert*>(a.get()))
            {
                server_message_alert* p = dynamic_cast<server_message_alert*>(a.get());
                std::cout << "msg: " << p->m_strMessage << std::endl;
            }
            else if (dynamic_cast<server_identity_alert*>(a.get()))
            {
                server_identity_alert* p = dynamic_cast<server_identity_alert*>(a.get());
                DBG("server_identity_alert: " << p->m_hServer << " name:  " << p->m_strName << " descr: " << p->m_strDescr);
            }
            else if (dynamic_cast<search_result_alert*>(a.get()))
            {
                search_result_alert* p = dynamic_cast<search_result_alert*>(a.get());
                // ok, prepare to get sources
                //p->m_result.dump();
                DBG("Results count: " << p->m_result.m_results_list.m_collection.size());

                if (p->m_result.m_more_results_avaliable)
                {
                    DBG("More results: ");
                    ses.post_search_more_result_request();
                }

                /*
                for (int n = 0; n < p->m_list.m_collection.size(); n++)
                {

                }
*/
                //if (p->m_list.m_collection.size() > 10)
                //{
                    // generate continue request
                    //search_request sr2;
                    //ses.post_search_more_result_request();
                //}

                int nIndex = p->m_result.m_results_list.m_collection.size() - 1;

                if (nIndex > 0)
                {

                    //DBG("Search related files");

                    //search_request sr2 = generateSearchRequest(p->m_list.m_collection[nIndex].m_hFile);
                    //ses.post_search_request(sr2);

                    /*

                    const boost::shared_ptr<base_tag> src = p->m_list.m_collection[nIndex].m_list.getTagByNameId(FT_COMPLETE_SOURCES);
                    const boost::shared_ptr<base_tag> sz = p->m_list.m_collection[nIndex].m_list.getTagByNameId(FT_FILESIZE);

                    if (src.get() && sz.get())
                    {
                        DBG("Complete sources: " << src.get()->asInt());
                        DBG("Size: " << sz.get()->asInt());
                        ses.post_sources_request(p->m_list.m_collection[nIndex].m_hFile, sz.get()->asInt());
                        // request sources

                    }
                    */

                }
            }
            else if(dynamic_cast<peer_message_alert*>(a.get()))
            {
                peer_message_alert* p = dynamic_cast<peer_message_alert*>(a.get());
                DBG("MSG: ADDR: " << p->m_strAddress << " MSG " << p->m_strMessage);
            }
            else if (peer_captcha_request_alert* p = dynamic_cast<peer_captcha_request_alert*>(a.get()))
            {
                DBG("captcha request ");
            }
            else if (peer_captcha_result_alert* p = dynamic_cast<peer_captcha_result_alert*>(a.get()))
            {
                DBG("captcha result " << p->m_nResult);
            }
            else if (peer_connected_alert* p = dynamic_cast<peer_connected_alert*>(a.get()))
            {
                DBG("peer connected: " << p->m_strAddress << " status: " << p->m_active);
            }
            else
            {
                std::cout << "Unknown alert " << std::endl;
            }

            a = ses.pop_alert();
        }
    }

    return 0;
}



