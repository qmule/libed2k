#include <iostream>

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

    
    return 0;
}
