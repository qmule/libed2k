#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "log.hpp"
#include "session.hpp"
#include "session_settings.hpp"
#include "alert_types.hpp"

using namespace libed2k;

class alert_handler
{
public:
    alert_handler(session& ses) : m_ses(ses)
    {

    }

    void operator()(server_connection_initialized_alert& a)
    {
        DBG("connection initialized: " << a.m_nClientId);
    }

    void operator()(server_status_alert& a)
    {
        DBG("server status: files count: " << a.m_nFilesCount << " users count " << a.m_nUsersCount);
    }

    void operator()(server_message_alert& a)
    {
        DBG("Message: " << a.m_strMessage);
    }

    void operator()(server_identity_alert& a)
    {
        DBG("server_identity_alert: " << a.m_hServer << " name:  " << a.m_strName << " descr: " << a.m_strDescr);
    }

    void operator()(search_result_alert& a)
    {

    }

    void operator()(server_connection_failed& a)
    {
        DBG("server connection failed: " << a.m_error.message());
    }


    void operator()(mule_listen_failed_alert& a)
    {
        DBG("listen failed");
    }

    session m_ses;

};

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
    settings.server_hostname = argv[1];
    libed2k::session ses(print, "0.0.0.0", settings);
    ses.set_alert_mask(alert::all_categories);

    libed2k::search_request sr;


    sr.add_entry(libed2k::search_request_entry("file1"));

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

    std::cout << "---- libed2k_client started\n"
              << "---- press q to exit\n"
              << "---- press something other for process alerts " << std::endl;

    while (std::cin.get() != 'q')
    {
        std::auto_ptr<alert> a = ses.pop_alert();

        alert_handler h(ses);

        while(a.get())
        {
            /*
            handle_alert<server_connection_initialized_alert,
            server_status_alert,
            server_message_alert,
            server_identity_alert,
            search_result_alert,
            server_connection_failed,
            mule_listen_failed_alert>::handle_alert(a, h);
            */

            if (dynamic_cast<server_connection_initialized_alert*>(a.get()))
            {
                server_connection_initialized_alert* p = dynamic_cast<server_connection_initialized_alert*>(a.get());
                std::cout << "server initalized: cid: "
                        << p->m_nClientId
                        << std::endl;
                DBG("send search request");
                //ses.post_search_request(sr);
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
                p->m_list.dump();
                DBG("Results count: " << p->m_list.m_collection.size());
                /*
                for (int n = 0; n < p->m_list.m_collection.size(); n++)
                {

                }
*/
                int nIndex = p->m_list.m_collection.size()/2;

                if (nIndex)
                {

                    const boost::shared_ptr<base_tag> src = p->m_list.m_collection[nIndex].m_list.getTagByNameId(FT_COMPLETE_SOURCES);
                    const boost::shared_ptr<base_tag> sz = p->m_list.m_collection[nIndex].m_list.getTagByNameId(FT_FILESIZE);

                    if (src.get() && sz.get())
                    {
                        DBG("Complete sources: " << src.get()->asInt());
                        DBG("Size: " << sz.get()->asInt());
                        ses.post_sources_request(p->m_list.m_collection[nIndex].m_hFile, sz.get()->asInt());
                        // request sources

                    }
                }
            }
            else if (dynamic_cast<server_connection_failed*>(a.get()))
            {
                server_connection_failed* p =  dynamic_cast<server_connection_failed*>(a.get());
                DBG("server connection failed: " << p->m_error.message());
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



