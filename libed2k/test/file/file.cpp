#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
#include <libtorrent/utf8.hpp>

#include "constants.hpp"
#include "packet_struct.hpp"
#include "log.hpp"
#include "file.hpp"
#include "archive.hpp"
#include "types.hpp"

using namespace libed2k;

int main(int argc, char* argv[])
{
    LOGGER_INIT()

    if (argc < 2)
    {
        ERR("Set filename in first parameter");
        return 1;
    }

    std::wstring strPath = L"C:/work";

    fs::wpath wp(strPath);

    if ( fs::exists( wp ) )
    {
        fs::wrecursive_directory_iterator end_itr; // default construction yields past-the-end
        for ( fs::wrecursive_directory_iterator itr( wp );  itr != end_itr;  ++itr )
        {            
            std::cout << "x\n";
            std::string strUTF8;
            libtorrent::wchar_utf8(itr->filename(), strUTF8);
            std::cout << strUTF8 << std::endl;
        }
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
