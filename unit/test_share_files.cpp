#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <fstream>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem.hpp>
#include <locale.h>

#include "libed2k/constants.hpp"
#include "libed2k/file.hpp"
#include "libed2k/log.hpp"
#include "libed2k/transfer.hpp"
#include "libed2k/deadline_timer.hpp"

namespace fs = boost::filesystem;

namespace libed2k
{

#define TCOUNT 3

    class test_transfer_params_maker : public transfer_params_maker
    {
    public:
        static error_code m_errors[TCOUNT];
        static int m_total[TCOUNT];
        static int m_progress[TCOUNT];

        test_transfer_params_maker(const std::string& known_file);
    protected:
        void process_item(hash_handle* ph);
    private:
        int m_index;
    };

    error_code test_transfer_params_maker::m_errors[]  = { errors::no_error, errors::filesize_is_zero, errors::file_was_truncated };
    int test_transfer_params_maker::m_total[]   = { 10, 20, 30 };
    int test_transfer_params_maker::m_progress[]= { 10, 0, 23 };

    test_transfer_params_maker::test_transfer_params_maker(const std::string& known_file) : transfer_params_maker(known_file), m_index(0) {}

    void test_transfer_params_maker::process_item(hash_handle* ph)
    {
        DBG("process item " << m_index);
        hash_status hs;
        hs.m_error = m_errors[m_index];
        hs.m_progress = std::make_pair(m_progress[m_index], m_total[m_index]);
        ph->set_status(hs);
        ++m_index;
        m_index = m_index % TCOUNT;
    }
}

BOOST_AUTO_TEST_SUITE(test_share_files)

const char chRussianDirectory[] = {'\xEF', '\xBB', '\xBF', '\xD1', '\x80', '\xD1', '\x83', '\xD1', '\x81', '\xD1', '\x81', '\xD0', '\xBA', '\xD0', '\xB0', '\xD1', '\x8F', '\x20', '\xD0', '\xB4', '\xD0', '\xB8', '\xD1', '\x80', '\xD0', '\xB5', '\xD0', '\xBA', '\xD1', '\x82', '\xD0', '\xBE', '\xD1', '\x80', '\xD0', '\xB8', '\xD1', '\x8F', '\x00' };
const char chRussianFilename[] = { '\xD1', '\x80', '\xD1', '\x83', '\xD1', '\x81', '\xD1', '\x81', '\xD0', '\xBA', '\xD0', '\xB8', '\xD0', '\xB9', '\x20', '\xD1', '\x84', '\xD0', '\xB0', '\xD0', '\xB9', '\xD0', '\xBB', '\x00' };

void generate_file(size_t nSize, const char* pchFilename)
{
    std::ofstream of(pchFilename, std::ios_base::binary | std::ios_base::out);

    if (of)
    {
        // generate small file
        for (size_t i = 0; i < nSize; i++)
        {
            of << 'X';
        }
    }
}

void create_directory_tree()
{
    std::string strDirectory = chRussianDirectory;
    std::string strFilename  = chRussianFilename;
    fs::path p(libed2k::convert_to_native(libed2k::bom_filter(strDirectory)));
    fs::create_directories(p);
    p /= libed2k::convert_to_native(libed2k::bom_filter(strDirectory));
    fs::create_directories(p);

    fs::path p1 = p / libed2k::convert_to_native(libed2k::bom_filter(strFilename + "01.txt"));
    fs::path p2 = p / libed2k::convert_to_native(libed2k::bom_filter(strFilename + "02.txt"));
    fs::path p3 = p / libed2k::convert_to_native(libed2k::bom_filter(strFilename + "03.txt"));
    fs::path p4 = p / libed2k::convert_to_native(libed2k::bom_filter(strFilename + "04.txt"));
    fs::path p5 = p / libed2k::convert_to_native(libed2k::bom_filter(strFilename + "05.txt"));

    generate_file(100, p1.string().c_str());
    generate_file(libed2k::PIECE_SIZE, p2.string().c_str());
    generate_file(libed2k::PIECE_SIZE+1, p3.string().c_str());
    generate_file(libed2k::PIECE_SIZE*4, p4.string().c_str());
    generate_file(libed2k::PIECE_SIZE+4566, p5.string().c_str());

    std::ofstream fstr(p.string().c_str());

}

void drop_directory_tree()
{
    std::string strDirectory = chRussianDirectory;
    fs::path p(libed2k::convert_to_native(libed2k::bom_filter(strDirectory)));
    fs::remove_all(p);
}

BOOST_AUTO_TEST_CASE(test_string_conversions)
{
    setlocale(LC_CTYPE, "");
    std::string strDirectory = chRussianDirectory;
    std::string strNative = libed2k::convert_to_native(libed2k::bom_filter(strDirectory));

    if (CHECK_BOM(strDirectory.size(), strDirectory))
    {
        BOOST_CHECK_EQUAL(strDirectory.substr(3), libed2k::convert_from_native(strNative));
    }
}

BOOST_AUTO_TEST_CASE(test_concurrency)
{
    //LOGGER_INIT();
    std::vector<boost::shared_ptr<libed2k::hash_handle> > v;
    libed2k::test_transfer_params_maker maker("");
    maker.start();
    maker.stop();
    maker.stop();
    maker.start();
    v.push_back(maker.make_transfer_params(""));
    v.push_back(maker.make_transfer_params(""));
    v.push_back(maker.make_transfer_params(""));

    while(maker.queue_size())
    {
#ifdef WIN32
        Sleep(500);
#else
        sleep(1);
#endif
    }

    for (size_t i = 0; i < TCOUNT; ++i)
    {
        BOOST_CHECK(v[i].get()->status() ==
                libed2k::hash_status(libed2k::test_transfer_params_maker::m_errors[i],
                        std::make_pair(libed2k::test_transfer_params_maker::m_progress[i], libed2k::test_transfer_params_maker::m_total[i])));
    }
}

BOOST_AUTO_TEST_SUITE_END()
