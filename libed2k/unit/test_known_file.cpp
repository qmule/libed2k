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
#include "constants.hpp"
#include "types.hpp"
#include "file.hpp"

BOOST_AUTO_TEST_SUITE(test_known_files)

void generate_file(size_t nSize, const char* pchFilename)
{
    std::ofstream of(pchFilename, std::ios_base::binary);

    if (of)
    {
        // generate small file
        for (int i = 0; i < nSize; i++)
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
        // generate test files
        generate_file(100, "./test1.bin");
        generate_file(libed2k::PIECE_SIZE, "./test2.bin");
        generate_file(libed2k::PIECE_SIZE+1, "./test3.bin");
        generate_file(libed2k::PIECE_SIZE*4, "./test4.bin");
        generate_file(libed2k::PIECE_SIZE+4566, "./test5.bin");
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
}

BOOST_AUTO_TEST_SUITE_END()
