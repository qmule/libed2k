#include <fstream>
#include <string>
#include <set>
#include <boost/bind.hpp>
#include "libed2k/filesystem.hpp"
#include "libed2k/size_type.hpp"
#include "libed2k/utf8.hpp"



inline bool generate_test_file(libed2k::size_type filesize, const std::string& filename)
{
    bool res = false;
    std::ofstream of(filename.c_str(), std::ios_base::binary | std::ios_base::out);

    if (of)
    {
        // generate small file
        for (libed2k::size_type i = 0; i < filesize; ++i)
        {
            of << 'X';
        }
        res = true;
    }

    return res;
}

class test_files_holder
{
public:
    ~test_files_holder()
    {
        libed2k::error_code ec;
        std::for_each(m_files.begin(), m_files.end(), boost::bind(&libed2k::remove, _1, ec));
    }

    void hold(const std::string& filename) { m_files.insert(libed2k::convert_to_native(filename));}
private:
    std::set<std::string> m_files;
};
