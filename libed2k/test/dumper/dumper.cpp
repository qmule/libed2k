#include <iostream>

#include "libed2k/packet_struct.hpp"
#include "libed2k/log.hpp"
#include "libed2k/file.hpp"
#include "libed2k/archive.hpp"


int main(int argc, char* argv[])
{
    LOGGER_INIT()

    if (argc < 2)
    {
        std::cerr << "Set collection full name in first parameter" << std::endl;
        return 1;
    }

    std::ifstream ifs(argv[1], std::ios_base::binary);

    try
    {
        if (ifs)
        {
            libed2k::emule_collection ec;
            libed2k::archive::ed2k_iarchive ifa(ifs);
            ifa >> ec;
            ec.dump();
        }
        else
        {
            std::cerr << "Unable to open file: " << argv[1] << std::endl;
            return 1;
        }
    }
    catch(libed2k::libed2k_exception& e)
    {
        std::cerr << "Error " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
