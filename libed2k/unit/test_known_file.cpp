#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <fstream>
#include <boost/test/unit_test.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem.hpp>
#include <locale.h>
#include "constants.hpp"
#include "types.hpp"
#include "file.hpp"
#include "session_impl.hpp"
#include "log.hpp"

//#define HASH_TEST

namespace libed2k
{
    namespace aux
    {
        class session_impl_test : public session_impl_base
        {
        public:
            session_impl_test(const session_settings& settings);
            virtual transfer_handle add_transfer(add_transfer_params const&, error_code& ec);
            void stop();
        };

        session_impl_test::session_impl_test(const session_settings& settings) : session_impl_base(settings)
        {
            m_fmon.start();
        }

        transfer_handle session_impl_test::add_transfer(add_transfer_params const& t, error_code& ec)
        {
            return (transfer_handle(boost::weak_ptr<transfer>()));
        }

        void session_impl_test::stop()
        {
            DBG("fmon stop");
            m_fmon.stop();
            DBG("fmon stop complete");
        }


    }
}

BOOST_AUTO_TEST_SUITE(test_known_files)


void generate_file(size_t nSize, const char* pchFilename)
{
    std::ofstream of(pchFilename, std::ios_base::binary);

    if (of)
    {
        // generate small file
        for (size_t i = 0; i < nSize; i++)
        {
            of << 'X';
        }
    }
}

struct test_files
{
    test_files() :
                m_file1("./test1.bin"),
                m_file2("./test2.bin"),
                m_file3("./test3.bin"),
                m_file4("./test4.bin"),
                m_file5("./test5.bin")
    {
#ifdef LONG_TIME_TESTS
        // generate test files
        generate_file(100, "./test1.bin");
        generate_file(libed2k::PIECE_SIZE, "./test2.bin");
        generate_file(libed2k::PIECE_SIZE+1, "./test3.bin");
        generate_file(libed2k::PIECE_SIZE*4, "./test4.bin");
        generate_file(libed2k::PIECE_SIZE+4566, "./test5.bin");
#endif
    }

    ~test_files()
    {
        if (libed2k::fs::exists("./test1.bin"))
        {
            libed2k::fs::remove("./test1.bin");
        }

        if (libed2k::fs::exists("./test2.bin"))
        {
            libed2k::fs::remove("./test2.bin");
        }

        if (libed2k::fs::exists("./test3.bin"))
        {
            libed2k::fs::remove("./test3.bin");
        }

        if (libed2k::fs::exists("./test4.bin"))
        {
            libed2k::fs::remove("./test4.bin");
        }

        if (libed2k::fs::exists("./test5.bin"))
        {
            libed2k::fs::remove("./test5.bin");
        }
    }

    libed2k::known_file m_file1;
    libed2k::known_file m_file2;
    libed2k::known_file m_file3;
    libed2k::known_file m_file4;
    libed2k::known_file m_file5;
};

BOOST_FIXTURE_TEST_CASE(test_file_calculate, test_files)
{
#ifdef LONG_TIME_TESTS
    m_file1.init();
    BOOST_CHECK_EQUAL(m_file1.getPiecesCount(), 1);
    BOOST_CHECK_EQUAL(m_file1.getFileHash().toString(), std::string("1AA8AFE3018B38D9B4D880D0683CCEB5"));

    m_file2.init();
    BOOST_CHECK_EQUAL(m_file2.getPiecesCount(), 2);
    BOOST_CHECK_EQUAL(m_file2.getFileHash().toString(), std::string("E76BADB8F958D7685B4549D874699EE9"));

    m_file3.init();
    BOOST_CHECK_EQUAL(m_file3.getPiecesCount(), 2);
    BOOST_CHECK_EQUAL(m_file3.getFileHash().toString(), std::string("49EC2B5DEF507DEA73E106FEDB9697EE"));


    m_file4.init();
    BOOST_CHECK_EQUAL(m_file4.getPiecesCount(), 5);
    BOOST_CHECK_EQUAL(m_file4.getFileHash().toString(), std::string("9385DCEF4CB89FD5A4334F5034C28893"));

    m_file5.init();
    BOOST_CHECK_EQUAL(m_file5.getPiecesCount(), 2);
    BOOST_CHECK_EQUAL(m_file5.getFileHash().toString(), std::string("9C7F988154D2C9AF16D92661756CF6B2"));
#endif
}

BOOST_AUTO_TEST_CASE(test_session)
{
    LOGGER_INIT()
    libed2k::session_settings s;
#ifdef WIN32
    setlocale(LC_CTYPE, "");
    FILE* fp = fopen("D:\\work\\isclient\\libed2k\\win32\\Debug\\x.txt", "rb");
    s.m_known_file = "D:\\work\\isclient\\libed2k\\win32\\Debug\\known.met";

    if (fp)
    {
        char chBuffer[1024] = {0};
        int n = fread(chBuffer,1,50, fp);
        s.m_fd_list.push_back(std::make_pair(std::string(chBuffer), true));
        DBG("load dir: " << libed2k::convert_to_native(std::string(chBuffer)));
        fclose(fp);
    }
        
    //DBG("ANSI: " << libed2k::convert_to_native(s.m_known_file));
    //s.m_fd_list.push_back(std::make_pair("d:\\mule", false));
#else
    s.m_known_file = "./known.met";
    s.m_fd_list.push_back(std::make_pair("/home/apavlov/mule", true));
    s.m_fd_list.push_back(std::make_pair("/home/apavlov/", false));
#endif
    libed2k::aux::session_impl_test st(s);
    st.load_state();
    st.stop();
}

BOOST_AUTO_TEST_SUITE_END()
