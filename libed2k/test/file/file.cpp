#include <iostream>
#include <fstream>

#include "constants.hpp"
#include "packet_struct.hpp"
#include "log.hpp"
#include "file.hpp"
#include "archive.hpp"

using namespace libed2k;

int main(int argc, char* argv[])
{
    LOGGER_INIT()

    if (argc < 2)
    {
        ERR("Set filename in first parameter");
        return 1;
    }

    std::ifstream fs(argv[1], std::ios::binary);

    if (fs)
    {
        known_file_collection kfc;
        libed2k::archive::ed2k_iarchive ifa(fs);

        try
        {
            ifa >> kfc;
            kfc.dump();
        }
        catch(libed2k_exception& e)
        {
            ERR("Error: " << e.what());
        }
    }
    else
    {
        ERR("Unable to open file: " << argv[1]);
        return 1;
    }

    return 0;
}
