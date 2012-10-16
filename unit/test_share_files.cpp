#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <sstream>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem.hpp>
#include <locale.h>

#include "libed2k/constants.hpp"
#include "libed2k/file.hpp"
#include "libed2k/log.hpp"
#include "libed2k/transfer.hpp"
#include "libed2k/deadline_timer.hpp"
#include "common.hpp"

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

#define WAIT_TPM(x) while(x.queue_size()) {}

BOOST_AUTO_TEST_SUITE(test_share_files)

const char chRussianDirectory[] = {'\xEF', '\xBB', '\xBF', '\xD1', '\x80', '\xD1', '\x83', '\xD1', '\x81', '\xD1', '\x81', '\xD0', '\xBA', '\xD0', '\xB0', '\xD1', '\x8F', '\x20', '\xD0', '\xB4', '\xD0', '\xB8', '\xD1', '\x80', '\xD0', '\xB5', '\xD0', '\xBA', '\xD1', '\x82', '\xD0', '\xBE', '\xD1', '\x80', '\xD0', '\xB8', '\xD1', '\x8F', '\x00' };
const char chRussianFilename[] = { '\xD1', '\x80', '\xD1', '\x83', '\xD1', '\x81', '\xD1', '\x81', '\xD0', '\xBA', '\xD0', '\xB8', '\xD0', '\xB9', '\x20', '\xD1', '\x84', '\xD0', '\xB0', '\xD0', '\xB9', '\xD0', '\xBB', '\x00' };

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

    generate_test_file(100, p1.string().c_str());
    generate_test_file(libed2k::PIECE_SIZE, p2.string().c_str());
    generate_test_file(libed2k::PIECE_SIZE+1, p3.string().c_str());
    generate_test_file(libed2k::PIECE_SIZE*4, p4.string().c_str());
    generate_test_file(libed2k::PIECE_SIZE+4566, p5.string().c_str());

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
    std::vector<boost::shared_ptr<libed2k::hash_handle> > v;
    libed2k::test_transfer_params_maker maker("");
    maker.start();
    maker.stop();
    maker.stop();
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


BOOST_AUTO_TEST_CASE(test_add_transfer_params_maker)
{
    test_files_holder tfh;
    const size_t sz = 5;
    const char* filename = "./test_filename";
    std::pair<libed2k::size_type, libed2k::md4_hash> tmpl[sz] =
    {
        std::make_pair(100, libed2k::md4_hash::fromString("1AA8AFE3018B38D9B4D880D0683CCEB5")),
        std::make_pair(libed2k::PIECE_SIZE, libed2k::md4_hash::fromString("E76BADB8F958D7685B4549D874699EE9")),
        std::make_pair(libed2k::PIECE_SIZE+1, libed2k::md4_hash::fromString("49EC2B5DEF507DEA73E106FEDB9697EE")),
        std::make_pair(libed2k::PIECE_SIZE*4, libed2k::md4_hash::fromString("9385DCEF4CB89FD5A4334F5034C28893")),
        std::make_pair(libed2k::PIECE_SIZE+4566, libed2k::md4_hash::fromString("9C7F988154D2C9AF16D92661756CF6B2"))
    };

    for (size_t n = 0; n < sz; ++n)
    {
        std::stringstream s;
        s << filename << n;
        BOOST_REQUIRE(generate_test_file(tmpl[n].first, s.str()));
        tfh.hold(s.str());
    }

    libed2k::transfer_params_maker tpm("");
    tpm.start();

    std::vector<boost::shared_ptr<libed2k::hash_handle> > v;

    for (size_t n = 0; n < sz; ++n)
    {
        std::stringstream s;
        s << filename << n;
        v.push_back(tpm.make_transfer_params(s.str()));
    }

    // wait hasher completed
    WAIT_TPM(tpm)
    tpm.stop(); // wait last file will hashed

    for (size_t n = 0; n < sz; ++n)
    {
        std::stringstream s;
        s << filename << n;
        libed2k::hash_status hs = v[n].get()->status();

        BOOST_CHECK(hs.valid());
        BOOST_CHECK(hs.completed());
        BOOST_CHECK_MESSAGE(v[n].get()->atp().file_hash == tmpl[n].second, s.str());
    }

    // free resources
    v.clear();

    // check resource clean before hash
    for (size_t k = 0; k < 40; ++k)
    {
        for (size_t n = 0; n < sz; ++n)
        {
            v.push_back(tpm.make_transfer_params("some non-exists file"));
        }
    }

    // start hashing and free reources
    tpm.start();
    v.clear();

    const char* zero_filename = "zero_filename.txt";
    tfh.hold(zero_filename);
    BOOST_REQUIRE(generate_test_file(0, zero_filename));
    boost::shared_ptr<libed2k::hash_handle> zero_handle = tpm.make_transfer_params(zero_filename);
    WAIT_TPM(tpm)
    tpm.stop();
    BOOST_CHECK(zero_handle.get()->status().m_error == libed2k::errors::make_error_code(libed2k::errors::filesize_is_zero));

    tpm.start();
    boost::shared_ptr<libed2k::hash_handle> non_exists_handle = tpm.make_transfer_params("non_exists");
    WAIT_TPM(tpm)
    tpm.stop();
    BOOST_CHECK(non_exists_handle.get()->status().m_error);
    DBG("test_add_transfer_params_maker {completed}");
}

BOOST_AUTO_TEST_SUITE_END()
