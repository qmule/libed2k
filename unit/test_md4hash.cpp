#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#include <string>

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <fstream>
#include <boost/test/unit_test.hpp>
#include "libed2k/md4_hash.hpp"
#include "libed2k/hasher.hpp"
#include "libed2k/packet_struct.hpp"
#include "common.hpp"


BOOST_AUTO_TEST_SUITE(test_md4_hash)

struct test_md4_hash
{
    test_md4_hash()
    {
    }

    libed2k::md4_hash m_test;
};

BOOST_FIXTURE_TEST_CASE(test_conversion, test_md4_hash)
{
    std::string strHash1 = "000102030405060708090A0B0C0D0F0D";
    BOOST_REQUIRE(strHash1.size() == libed2k::MD4_HASH_SIZE*2);
    m_test = libed2k::md4_hash::fromString(strHash1);
    BOOST_CHECK_EQUAL(strHash1, m_test.toString());
    strHash1  = "000102030405F6c70b090a0B0c0D0f0D";
    std::string strHash1U = "000102030405F6C70B090A0B0C0D0F0D";
    m_test = libed2k::md4_hash::fromString(strHash1);
    BOOST_CHECK_EQUAL(strHash1U, m_test.toString());
    BOOST_CHECK(!libed2k::md4_hash::fromString(std::string("000102030405F6C7XB09KA0B0C0D0F0D")).defined());
    BOOST_CHECK_EQUAL(std::string("31D6CFE0D16AE931B73C59D7E0C089C0"), libed2k::md4_hash::terminal.toString());
    BOOST_CHECK_EQUAL(std::string("31D6CFE0D14CE931B73C59D7E0C04BC0"), libed2k::md4_hash::libed2k.toString());
    BOOST_CHECK_EQUAL(std::string("31D6CFE0D10EE931B73C59D7E0C06FC0"), libed2k::md4_hash::emule.toString());
    BOOST_CHECK_EQUAL(std::string("00000000000000000000000000000000"), libed2k::md4_hash::invalid.toString());
}

BOOST_AUTO_TEST_CASE(test_compare)
{
    libed2k::md4_hash h1 = libed2k::md4_hash::fromString("000102030405060708090A0B0C0D0F0D");
    libed2k::md4_hash h2 = libed2k::md4_hash::fromString("000102030405060708090A0B0C0D0F0D");
    libed2k::md4_hash h3 = libed2k::md4_hash::fromString("0A0102030405060708090A0B0C0D0F0D");
    BOOST_CHECK(h3[0] == '\x0A');
    BOOST_CHECK(h2[0] == '\x00');
    BOOST_CHECK(h1 == h2);
    BOOST_CHECK(h3 > h2);
}

BOOST_AUTO_TEST_CASE(test_user_agent)
{
    BOOST_CHECK_EQUAL(libed2k::uagent2csoft(libed2k::md4_hash::terminal), libed2k::SO_UNKNOWN);
    BOOST_CHECK_EQUAL(libed2k::uagent2csoft(libed2k::md4_hash::libed2k), libed2k::SO_LIBED2K);
    libed2k::md4_hash ml;
    ml[5]   = 'M';
    ml[14]  = 'L';
    libed2k::md4_hash em;
    em[5]   = 14;
    em[14]  = 111;
    BOOST_CHECK_EQUAL(libed2k::uagent2csoft(ml), libed2k::SO_MLDONKEY);
    BOOST_CHECK_EQUAL(libed2k::uagent2csoft(em), libed2k::SO_EMULE);
    BOOST_CHECK_EQUAL(libed2k::uagent2csoft(libed2k::md4_hash::emule), libed2k::SO_EMULE);
}

BOOST_AUTO_TEST_CASE(test_partial_hashing_and_hashset)
{
    test_files_holder tfh;
    std::vector<std::pair<libed2k::size_type, libed2k::md4_hash> > vtf;
    vtf.push_back(std::make_pair(100, libed2k::md4_hash::fromString("1AA8AFE3018B38D9B4D880D0683CCEB5")));
    vtf.push_back(std::make_pair(libed2k::PIECE_SIZE, libed2k::md4_hash::fromString("E76BADB8F958D7685B4549D874699EE9")));
    vtf.push_back(std::make_pair(libed2k::PIECE_SIZE+1, libed2k::md4_hash::fromString("49EC2B5DEF507DEA73E106FEDB9697EE")));
    vtf.push_back(std::make_pair(libed2k::PIECE_SIZE*4, libed2k::md4_hash::fromString("9385DCEF4CB89FD5A4334F5034C28893")));

    for (std::vector<std::pair<libed2k::size_type, libed2k::md4_hash> >::const_iterator itr = vtf.begin(); itr != vtf.end(); ++itr)
    {
        BOOST_REQUIRE(generate_test_file(itr->first, "./thfile.bin"));
        tfh.hold("./thfile.bin");

        char* chFullBuffer = new char[static_cast<unsigned int>(itr->first)];  // allocate buffer for file data
        FILE* fh = fopen("./thfile.bin", "rb");
        BOOST_REQUIRE(fh);
        BOOST_REQUIRE(fread(chFullBuffer, 1, static_cast<unsigned int>(itr->first), fh) == static_cast<size_t>(itr->first));
        fclose(fh);

        libed2k::size_type pieces = libed2k::div_ceil(itr->first, libed2k::PIECE_SIZE);
        std::vector<libed2k::md4_hash> part_hashset;
        part_hashset.resize(pieces);
        libed2k::size_type capacity = itr->first;

        // use hasher
        libed2k::hasher ih;

        for (int i = 0; i < pieces; ++i)
        {
            libed2k::size_type in_piece_capacity = (std::min)(libed2k::PIECE_SIZE, capacity);

            while(in_piece_capacity > 0)
            {
                ih.update((chFullBuffer + (itr->first - capacity)), (std::min)(libed2k::BLOCK_SIZE, in_piece_capacity));
                capacity -= (std::min)(libed2k::BLOCK_SIZE, in_piece_capacity);
                in_piece_capacity -= (std::min)(libed2k::BLOCK_SIZE, in_piece_capacity);
            }

            part_hashset[i] = ih.final();
            ih.reset();
        }

        if (pieces*libed2k::PIECE_SIZE == itr->first)
        {
            part_hashset.push_back(libed2k::md4_hash::terminal);
        }

        BOOST_CHECK_EQUAL(libed2k::md4_hash::fromHashset(part_hashset), itr->second);
        delete chFullBuffer;
    }
}

BOOST_AUTO_TEST_SUITE_END()
