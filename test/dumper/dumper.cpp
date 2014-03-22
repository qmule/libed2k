#include <iostream>
#include <fstream>
#include "libed2k/packet_struct.hpp"
#include "libed2k/log.hpp"
#include "libed2k/file.hpp"
#include "libed2k/search.hpp"

int main(int argc, char* argv[])
{
    LOGGER_INIT(LOG_ALL)

    if (argc < 2)
    {
        std::cerr << "Set search string in first parameters" << std::endl;
        return 1;
    }

    std::string src = argv[1];
    DBG("Incoming string: " << src);

    try
    {
        libed2k::search_request sr = libed2k::generateSearchRequest(0,0,0,0, "", "", "", 0, 0, libed2k::convert_from_native(src));

        for(libed2k::search_request::const_iterator itr = sr.begin(); itr != sr.end(); ++itr)
        {
            DBG("Operand: {" << (itr->isLogic()?"true":"false") <<
                "} value: {" << libed2k::sre_operation2string(itr->getOperator()) <<
                "} str: {" << itr->getStrValue() << "}");
        }
    }
    catch(libed2k::libed2k_exception& e)
    {
        std::cerr << "Error on generate search request: " << e.what() << std::endl;
    }

    if (argc >= 3)
    {
        // process incoming server.met
        DBG("Process incoming server.met" << argv[2]);
        try
        {
            libed2k::server_met sm;
            std::ifstream ifs(argv[2], std::ios_base::binary);
            libed2k::archive::ed2k_iarchive ifa(ifs);
            ifa >> sm;
            DBG("Servers count: " << sm.m_servers.m_size);

            for(size_t n = 0; n < sm.m_servers.m_size; ++n)
            {
                DBG(sm.m_servers.m_collection.at(n).m_network_point.to_string());

                for (size_t k = 0; k < sm.m_servers.m_collection.at(n).m_list.size(); ++k)
                {
                    sm.m_servers.m_collection.at(n).m_list[k]->dump();
                }
            }
        }
        catch(libed2k::libed2k_exception& e)
        {
            ERR("Error on parse file " << e.what());
        }

    }

    
    return 0;
}
