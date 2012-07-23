#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#include <string>

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <boost/test/unit_test.hpp>
#include "libed2k/md4_hash.hpp"
#include <libtorrent/piece_picker.hpp>
#include "libed2k/peer_connection.hpp"

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
    BOOST_CHECK_THROW(h3[16], libed2k::libed2k_exception);
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
}

BOOST_AUTO_TEST_SUITE_END()
