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

        while(a.get())
        {
            if (dynamic_cast<server_connection_initialized_alert*>(a.get()))
            {
                server_connection_initialized_alert* p = dynamic_cast<server_connection_initialized_alert*>(a.get());
                std::cout << "server initalized: cid: "
                        << p->m_nClientId
                        << " fc: " << p->m_nFilesCount
                        << " uc: " << p->m_nUsersCount << std::endl;
                DBG("send search request");
                ses.post_search_request(sr);
            }
            else if (dynamic_cast<server_message_alert*>(a.get()))
            {
                server_message_alert* p = dynamic_cast<server_message_alert*>(a.get());
                std::cout << "msg: " << p->m_strMessage << std::endl;
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
            else
            {
                std::cout << "Unknown alert " << std::endl;
            }

            a = ses.pop_alert();
        }
    }

    return 0;
}



