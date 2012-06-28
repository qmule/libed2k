#include <iostream>

#include "libed2k/packet_struct.hpp"
#include "libed2k/log.hpp"
#include "libed2k/file.hpp"

int main(int argc, char* argv[])
{
    LOGGER_INIT(LOG_ALL)

    if (argc < 2)
    {
        std::cerr << "Set collection full name in first parameter" << std::endl;
        return 1;
    }


    libed2k::emule_collection ecoll = libed2k::emule_collection::fromFile(argv[1]);
    
    return 0;
}
