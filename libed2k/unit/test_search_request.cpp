#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <boost/test/unit_test.hpp>
#include "libed2k/packet_struct.hpp"
#include "libed2k/util.hpp"
#include "libed2k/log.hpp"
#include "libed2k/file.hpp"
#include "libed2k/search.hpp"

BOOST_AUTO_TEST_SUITE(test_search_request)

BOOST_AUTO_TEST_CASE(test_search_build)
{
    // simple search one string with logical expressions
    libed2k::search_request r1 =  libed2k::generateSearchRequest(0, 0, 0, 0, "", "", "", 0, 0, "X1 AND X2 AND X3 NOT X4 OR X5");

    BOOST_REQUIRE_EQUAL(r1.size(), 9);
    BOOST_CHECK(r1[0].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_OR));
    BOOST_CHECK(r1[1].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_NOT));
    BOOST_CHECK(r1[2].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK(r1[3].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK_EQUAL(r1[4].getStrValue(), std::string("X1"));
    BOOST_CHECK_EQUAL(r1[5].getStrValue(), std::string("X2"));
    BOOST_CHECK_EQUAL(r1[6].getStrValue(), std::string("X3"));
    BOOST_CHECK_EQUAL(r1[7].getStrValue(), std::string("X4"));
    BOOST_CHECK_EQUAL(r1[8].getStrValue(), std::string("X5"));

    // check quotes and auto
    libed2k::search_request r2 = libed2k::generateSearchRequest(0,0,0,0,"", "", "", 0, 0, "X1 \"AND\"");
    BOOST_REQUIRE_EQUAL(r2.size(), 3);
    BOOST_CHECK(r2[0].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK_EQUAL(r2[1].getStrValue(), std::string("X1"));
    BOOST_CHECK_EQUAL(r2[2].getStrValue(), std::string("AND"));

    // check incorrect expressions
    BOOST_CHECK_THROW(libed2k::generateSearchRequest(0,0,0,0, "", "", "", 0, 0, "X1 AND"), libed2k::libed2k_exception);
    BOOST_CHECK_THROW(libed2k::generateSearchRequest(0,0,0,0, "", "", "", 0, 0, "AND X1"), libed2k::libed2k_exception);
    BOOST_CHECK_THROW(libed2k::generateSearchRequest(0,0,0,0, "", "", "", 0, 0, "X1 AND OR DATA"), libed2k::libed2k_exception);
    BOOST_CHECK_THROW(libed2k::generateSearchRequest(0,0,0,0, "", "", "", 0, 0, "X1 \"DATA   "), libed2k::libed2k_exception);
    BOOST_CHECK_THROW(libed2k::generateSearchRequest(0,0,0,0, "", "", "", 0, 0, "AND"), libed2k::libed2k_exception);
    BOOST_CHECK_THROW(libed2k::generateSearchRequest(0,0,0,0, "", "", "", 0, 0, "X1 \"AND\"\"DATA"), libed2k::libed2k_exception);
    BOOST_CHECK_THROW(libed2k::generateSearchRequest(0,0,0,0, "", "", "", 0, 0, "X1 NOT"), libed2k::libed2k_exception);
    BOOST_CHECK_THROW(libed2k::generateSearchRequest(0,0,0,0, "", "", "", 0, 0, "X1 OR"), libed2k::libed2k_exception);

    // check correct expression in case we have some prev parameters before query - first operand accepted
    BOOST_CHECK_NO_THROW(libed2k::generateSearchRequest(40, 70, 20, 0, libed2k::ED2KFTSTR_AUDIO, "", "", 0, 0, "NOT X1"));


    // check spaces truncating and linked values
    libed2k::search_request r3 = libed2k::generateSearchRequest(0,0,0,0, "", "", "", 0, 0, "X1  \"AND   \"  OR     XDATA  \"M\"M\"M\"   NOT   AAA");
    BOOST_REQUIRE_EQUAL(r3.size(), 9);
    BOOST_CHECK(r3[0].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_NOT));
    BOOST_CHECK(r3[1].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK(r3[2].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_OR));
    BOOST_CHECK(r3[3].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK_EQUAL(r3[4].getStrValue(), std::string("X1"));
    BOOST_CHECK_EQUAL(r3[5].getStrValue(), std::string("AND   "));
    BOOST_CHECK_EQUAL(r3[6].getStrValue(), std::string("XDATA"));
    BOOST_CHECK_EQUAL(r3[7].getStrValue(), std::string("MMM"));
    BOOST_CHECK_EQUAL(r3[8].getStrValue(), std::string("AAA"));

    // check special tag expression
    libed2k::search_request r4 = libed2k::generateSearchRequest(10,90,1,0, "", "", "", 0, 0, "X1 OR X2");
    BOOST_REQUIRE_EQUAL(r4.size(), 9);
    BOOST_CHECK(r4[0].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_OR));
    BOOST_CHECK(r4[1].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK(r4[2].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK(r4[3].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK_EQUAL(r4[4].getInt32Value(), 10);
    BOOST_CHECK_EQUAL(r4[5].getInt32Value(), 90);
    BOOST_CHECK_EQUAL(r4[6].getInt32Value(), 1);
    BOOST_CHECK_EQUAL(r4[7].getStrValue(), std::string("X1"));
    BOOST_CHECK_EQUAL(r4[8].getStrValue(), std::string("X2"));

    // check full expression
    libed2k::search_request r5 = libed2k::generateSearchRequest(10,9999000000000UL,300,0, libed2k::ED2KFTSTR_CDIMAGE, "kad", "", 0, 0, "X1 OR X2 and ");
    BOOST_REQUIRE_EQUAL(r5.size(), 15);
    BOOST_CHECK(r5[0].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK(r5[1].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_OR));
    BOOST_CHECK(r5[2].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK(r5[3].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK(r5[4].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK(r5[5].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK(r5[6].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK_EQUAL(r5[7].getStrValue(), libed2k::ED2KFTSTR_PROGRAM);
    BOOST_CHECK_EQUAL(r5[8].getInt32Value(), 10);
    BOOST_CHECK_EQUAL(r5[9].getInt64Value(), 9999000000000UL);
    BOOST_CHECK_EQUAL(r5[10].getInt32Value(), 300);
    BOOST_CHECK_EQUAL(r5[11].getStrValue(), std::string("kad"));
    BOOST_CHECK_EQUAL(r5[12].getStrValue(), std::string("X1"));
    BOOST_CHECK_EQUAL(r5[13].getStrValue(), std::string("X2"));
    BOOST_CHECK_EQUAL(r5[14].getStrValue(), std::string("and"));
}

BOOST_AUTO_TEST_CASE(test_search_users_folders)
{
    // simple search one string with logical expressions
    libed2k::search_request r1 =  libed2k::generateSearchRequest(40, 70, 20,0, libed2k::ED2KFTSTR_USER, "", "", 0, 0, "X1 X2");
    BOOST_REQUIRE_EQUAL(r1.size(), 5);
    BOOST_CHECK(r1[0].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK(r1[1].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK_EQUAL(r1[2].getStrValue(), std::string("'+++USERNICK+++'"));
    BOOST_CHECK_EQUAL(r1[3].getStrValue(), std::string("X1"));
    BOOST_CHECK_EQUAL(r1[4].getStrValue(), std::string("X2"));

    libed2k::search_request r2 =  libed2k::generateSearchRequest(40, 70, 20,0, libed2k::ED2KFTSTR_FOLDER, "", "", 0, 0, "X1");
    BOOST_REQUIRE_EQUAL(r2.size(), 5);
    BOOST_CHECK(r2[0].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_AND));
    BOOST_CHECK(r2[1].getOperator() == static_cast<libed2k::search_request_entry::SRE_Operation>(libed2k::search_request_entry::SRE_NOT));
    BOOST_CHECK_EQUAL(r2[2].getStrValue(), std::string(libed2k::ED2KFTSTR_EMULECOLLECTION));
    BOOST_CHECK_EQUAL(r2[3].getStrValue(), std::string("ED2K:\\"));
    BOOST_CHECK_EQUAL(r2[4].getStrValue(), std::string("X1"));

    BOOST_CHECK_THROW(libed2k::generateSearchRequest(40, 70, 20,0, libed2k::ED2KFTSTR_USER, "", "", 0, 0, "AND X1"), libed2k::libed2k_exception);
}

BOOST_AUTO_TEST_CASE(test_limits)
{
    // long string
    BOOST_CHECK_THROW(libed2k::generateSearchRequest(40, 70, 20,0, libed2k::ED2KFTSTR_AUDIO, "1234567890122345678900000", "", 0, 0, "X1"), libed2k::libed2k_exception);
    // too complex
    BOOST_CHECK_THROW(libed2k::generateSearchRequest(40, 70, 20,0, libed2k::ED2KFTSTR_AUDIO, "", "", 0, 0, "X1 X2 X3 x4 x5 x6 x7 x8 x9 x10 x11 x12 x13 x14 x15 y z d NOT K"), libed2k::libed2k_exception);
}

BOOST_AUTO_TEST_SUITE_END()
