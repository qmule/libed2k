#define BOOST_TEST_DYN_LINK
#include <string>

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <boost/test/unit_test.hpp>
#include "md4_hash.hpp"

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
    m_test.fromString(strHash1);
    BOOST_CHECK_EQUAL(strHash1, m_test.toString());
    strHash1  = "000102030405F6c70b090a0B0c0D0f0D";
    std::string strHash1U = "000102030405F6C70B090A0B0C0D0F0D";
    m_test.fromString(strHash1);
    BOOST_CHECK_EQUAL(strHash1U, m_test.toString());
    BOOST_CHECK_THROW(m_test.fromString(std::string("000102030405F6C7XB09KA0B0C0D0F0D")), libed2k::libed2k_exception);
}

BOOST_AUTO_TEST_CASE(test_compare)
{
    libed2k::md4_hash h1("000102030405060708090A0B0C0D0F0D");
    libed2k::md4_hash h2("000102030405060708090A0B0C0D0F0D");
    libed2k::md4_hash h3("0A0102030405060708090A0B0C0D0F0D");
    BOOST_CHECK(h3[0] == '\x0A');
    BOOST_CHECK(h2[0] == '\x00');
    BOOST_CHECK(h1 == h2);
    BOOST_CHECK(h3 > h2);
    BOOST_CHECK_THROW(h3[16], libed2k::libed2k_exception);
}

BOOST_AUTO_TEST_SUITE_END()
