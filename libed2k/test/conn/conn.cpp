#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "base_socket.hpp"
#include "log.hpp"
#include "session.hpp"
#include "session_settings.hpp"
#include "alert_types.hpp"

using namespace libed2k;

int main(int argc, char* argv[])
{
    init_logs();

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

    sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_AND));
    sr.add_entry(libed2k::search_request_entry("song"));
    sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_AND));
    sr.add_entry(libed2k::search_request_entry(FT_FILESIZE, ED2K_SEARCH_OP_GREATER, 300));
    sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_NOT));
    //sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_OR));
    //sr.add_entry(libed2k::search_request_entry(search_request_entry::SRE_AND));
    sr.add_entry(libed2k::search_request_entry("ice"));
    sr.add_entry(libed2k::search_request_entry("kkkkJKJ"));

    std::cout << "---- libed2k_client started\n"
              << "---- press q to exit\n"
              << "---- press something other for process alerts " << std::endl;
    while (std::cin.get() != 'q')
    {
        std::auto_ptr<alert> a = ses.pop_alert();

        while(a.get())
        {
            if (dynamic_cast<a_server_connection_initialized*>(a.get()))
            {
                a_server_connection_initialized* p = dynamic_cast<a_server_connection_initialized*>(a.get());
                std::cout << "server initalized: cid: "
                        << p->m_nClientId
                        << " fc: " << p->m_nFilesCount
                        << " uc: " << p->m_nUsersCount << std::endl;
                DBG("send search request");
                ses.post_search_request(sr);
            }
            else if (dynamic_cast<a_server_message*>(a.get()))
            {
                a_server_message* p = dynamic_cast<a_server_message*>(a.get());
                std::cout << "msg: " << p->m_strMessage << std::endl;
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

