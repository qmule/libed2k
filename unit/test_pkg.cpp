#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#include <string>

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <fstream>
#include <boost/test/unit_test.hpp>
#include "libed2k/packet_struct.hpp"
#include "common.hpp"


BOOST_AUTO_TEST_SUITE(test_pkg)

BOOST_AUTO_TEST_CASE(test_extract_message){
    libed2k::error_code ec;
    libed2k::message msg;

    char incorrect_packet[] = {libed2k::OP_EDONKEYHEADER, '\x00'};
    msg = libed2k::extract_message(incorrect_packet, sizeof(incorrect_packet), ec);
    BOOST_CHECK_EQUAL(ec, libed2k::errors::make_error_code(libed2k::errors::invalid_packet_size));

    char packet1[] = {libed2k::OP_EDONKEYPROT, '\x01', '\x00', '\x00', '\x00', libed2k::OP_HELLO };
    msg = libed2k::extract_message(packet1, sizeof(packet1), ec);
    BOOST_CHECK_EQUAL(ec, libed2k::errors::make_error_code(libed2k::errors::no_error));
    BOOST_CHECK(msg.second.empty());

    char packet11[] = {libed2k::OP_EDONKEYPROT, '\x05', '\x00', '\x00', '\x00', libed2k::OP_HELLO, '\x31', '\x32', '\x33', '\x34'};
    msg = libed2k::extract_message(packet11, sizeof(packet11), ec);
    BOOST_CHECK_EQUAL(ec, libed2k::errors::make_error_code(libed2k::errors::no_error));
    BOOST_CHECK_EQUAL(msg.second, std::string("1234"));

    char packet2[] = {libed2k::OP_EDONKEYPROT, '\x09', '\x00', '\x00', '\x00', libed2k::OP_HELLO };
    msg = libed2k::extract_message(packet2, sizeof(packet2), ec);
    BOOST_CHECK_EQUAL(ec, libed2k::errors::make_error_code(libed2k::errors::invalid_packet_size));

    char packet3[] = {'\xFF', '\x09', '\x00', '\x00', '\x00', libed2k::OP_HELLO };
    msg = libed2k::extract_message(packet3, sizeof(packet3), ec);
    BOOST_CHECK_EQUAL(ec, libed2k::errors::make_error_code(libed2k::errors::invalid_protocol_type));

    char packet4[] = {libed2k::OP_UDPRESERVEDPROT1, '\x09', '\x00', '\x00', '\x00', libed2k::OP_HELLO };
    msg = libed2k::extract_message(packet4, sizeof(packet4), ec);
    BOOST_CHECK_EQUAL(ec, libed2k::errors::make_error_code(libed2k::errors::unsupported_udp_res1_type));

}

BOOST_AUTO_TEST_SUITE_END()
