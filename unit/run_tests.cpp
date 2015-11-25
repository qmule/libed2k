#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#define BOOST_TEST_MODULE main
#ifndef _AIX
#include <boost/test/included/unit_test.hpp>
#endif
#include <boost/test/unit_test.hpp>

#include "libed2k/error_code.hpp"
#include "libed2k/ctag.hpp"
#include "libed2k/file.hpp"
#include "libed2k/transfer_handle.hpp"
#include "libed2k/peer_request.hpp"
#include "libed2k/util.hpp"

extern std::vector<libed2k::peer_request> mk_peer_requests(
    libed2k::size_type begin, libed2k::size_type end, libed2k::size_type fsize);

BOOST_AUTO_TEST_SUITE(simple_exception_test)

struct simple_exception
{
    void throw_no_error()
    {
        throw libed2k::libed2k_exception(libed2k::error_code(libed2k::errors::no_error, libed2k::get_libed2k_category()));
    }

    void throw_tag_error()
    {
        throw libed2k::libed2k_exception(libed2k::error_code(libed2k::errors::tag_type_mismatch, libed2k::get_libed2k_category()));
    }

    void throw_unknown_error()
    {
        throw libed2k::libed2k_exception(libed2k::error_code(10000, libed2k::get_libed2k_category()));
    }
};

BOOST_FIXTURE_TEST_CASE(test_libed2k_exceptions, simple_exception)
{
    BOOST_CHECK_THROW (throw_no_error(), libed2k::libed2k_exception);
    BOOST_CHECK_THROW (throw_unknown_error(), libed2k::libed2k_exception);

    try
    {
        throw_tag_error();
    }
    catch(libed2k::libed2k_exception& e)
    {
        BOOST_CHECK_EQUAL(e.what(), libed2k::get_libed2k_category().message(libed2k::errors::tag_type_mismatch));
    }
}

BOOST_AUTO_TEST_CASE(test_to_string_functions)
{
    libed2k::tg_type nType = libed2k::TAGTYPE_STR22;
    BOOST_CHECK_EQUAL(libed2k::tagTypetoString(nType), std::string("TAGTYPE_STR22"));
}


BOOST_AUTO_TEST_CASE(test_file_functions)
{
    // extensions must be in lower case
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeID("ff.song.mP3"), libed2k::ED2KFT_ANY);
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeID("..dfdf..song.AVi"), libed2k::ED2KFT_ANY);
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeID("dff/..fdsong.sdsdmp3"), libed2k::ED2KFT_ANY);
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeID("dfdf.songmp3"), libed2k::ED2KFT_ANY);
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeID(".dddfdf.song.DOC"), libed2k::ED2KFT_ANY);

    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeSearchTerm(libed2k::GetED2KFileTypeID("xxx.mp3")), libed2k::ED2KFTSTR_AUDIO);
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeSearchTerm(libed2k::GetED2KFileTypeID("xxx.avi")), libed2k::ED2KFTSTR_VIDEO);
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeSearchTerm(libed2k::GetED2KFileTypeID("xxx.rar")), libed2k::ED2KFTSTR_PROGRAM);
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeID(".dddfdf.song.doc"), libed2k::ED2KFT_DOCUMENT);
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeID(".dddfdf.song.iso"), libed2k::ED2KFT_CDIMAGE);


}

BOOST_AUTO_TEST_CASE(div_ceil_test)
{
    BOOST_CHECK_EQUAL(libed2k::div_ceil(10,2), 5);
    BOOST_CHECK_EQUAL(libed2k::div_ceil(10,3), 4);
    BOOST_CHECK_EQUAL(libed2k::div_ceil(13,2), 7);
    boost::uint64_t n = 0;
    BOOST_CHECK_EQUAL(libed2k::div_ceil(0, libed2k::PIECE_SIZE), 0);
    BOOST_CHECK_EQUAL(libed2k::div_ceil(n, libed2k::PIECE_SIZE), 0U);
}

BOOST_AUTO_TEST_CASE(range_test)
{
    using namespace libed2k;

    range<int> rng1(std::make_pair(0,10));
    BOOST_CHECK(!rng1.empty());
    rng1 -= std::make_pair(5, 7);
    BOOST_CHECK(!rng1.empty());
    rng1 -= std::make_pair(0, 3);
    BOOST_CHECK(!rng1.empty());
    rng1 -= std::make_pair(3, 5);
    BOOST_CHECK(!rng1.empty());
    rng1 -= std::make_pair(8, 10);
    BOOST_CHECK(!rng1.empty());
    rng1 -= std::make_pair(7, 8);
    BOOST_CHECK(rng1.empty());
}

BOOST_AUTO_TEST_CASE(check_error_codes_msg_ranges)
{
    // check error msgs contain appropriate messages count
    libed2k::error_code e = libed2k::error_code(libed2k::errors::num_errors - 1,  libed2k::get_libed2k_category());
    BOOST_CHECK(!e.message().empty());
}

BOOST_AUTO_TEST_CASE(check_url_decode)
{
    BOOST_CHECK_EQUAL(libed2k::url_decode("%20"), " ");
    BOOST_CHECK_EQUAL(libed2k::url_decode("05%20-%20"), "05 - ");
    BOOST_CHECK_EQUAL(libed2k::url_decode(""), "");
    BOOST_CHECK_EQUAL(libed2k::url_decode("xlc"), "xlc");
    BOOST_CHECK_EQUAL(libed2k::url_decode("%20%20xlc"), "  xlc");
    BOOST_CHECK_EQUAL(libed2k::url_decode("%20A"), " A");
    BOOST_CHECK_EQUAL(libed2k::url_encode(" A "), "%20A%20");
    BOOST_CHECK_EQUAL(libed2k::url_encode("A0123456789B,>."), "A0123456789B%2C%3E%2E");
}

BOOST_AUTO_TEST_CASE(check_collection_dir)
{
    BOOST_CHECK_EQUAL(libed2k::collection_dir("dir1-dir2-5.emulecollection"), "dir1\\dir2");
    BOOST_CHECK_EQUAL(libed2k::collection_dir("dir1-dir2_2-50.emulecollection"), "dir1\\dir2_2");
    BOOST_CHECK_EQUAL(libed2k::collection_dir("xxx.avi"), "");
}

BOOST_AUTO_TEST_CASE(check_dat_filter_parser)
{
    const char* correct_lines[] =
    {
            "0.0.0.0         - 5.79.127.255    , 0   , Non-IS Net",
            "5.79.128.0      - 5.79.255.255    , 255 , IS Net",
            "5.80.0.0        - 5.205.255.255   , 0   , Non-IS Net",
            "5.206.0.0       - 5.206.127.255   , 255 , IS Net",
            "5.206.128.0     - 10.47.255.255   , 0   , Non-IS Net",
            "10.48.0.0       - 10.63.255.255   , 255 , IS Net",
            "10.64.0.0       - 10.99.255.255   , 0   , Non-IS Net",
            "10.100.0.0      - 10.100.255.255  , 255 , IS Net",
            "10.101.0.0      - 31.207.127.255  , 0   , Non-IS Net",
            "31.207.128.0    - 31.207.255.255  , 255 , IS Net",
            "31.208.0.0      - 37.139.255.255  , 0   , Non-IS Net",
            "37.140.0.0      - 37.140.127.255  , 255 , IS Net",
            "37.140.128.0    - 77.222.95.255   , 0   , Non-IS Net",
            "77.222.96.0     - 77.222.127.255  , 255 , IS Net",
            "77.222.128.0    - 78.28.255.255   , 0   , Non-IS Net",
            "78.29.0.0       - 78.29.63.255    , 255 , IS Net",
            "78.29.64.0      - 80.255.79.255   , 0   , Non-IS Net",
            "80.255.80.0     - 80.255.95.255   , 255 , IS Net",
            "80.255.96.0     - 83.142.159.255  , 0   , Non-IS Net",
            "83.142.160.0    - 83.142.167.255  , 255 , IS Net",
            "83.142.168.0    - 88.205.255.255  , 0   , Non-IS Net",
            "88.206.0.0      - 88.206.127.255  , 255 , IS Net",
            "88.206.128.0    - 94.24.127.255   , 0   , Non-IS Net",
            "94.24.128.0     - 94.24.255.255   , 255 , IS Net",
            "94.24.255.255   - 109.190.255.255 , 0   , Non-IS Net",
            "109.191.0.0     - 109.191.255.255 , 255 , IS Net",
            "109.192.0.0     - 176.226.127.255 , 0   , Non-IS Net",
            "176.226.128.0   - 176.226.255.255 , 255 , IS Net",
            "176.227.0.0     - 192.167.255.255 , 0   , Non-IS Net",
            "192.168.0.0     - 192.168.255.255 , 255 , IS Net",
            "192.169.0.0     - 255.255.255.255 , 0   , Non-IS Net"
    };


    for(size_t i = 0; i < sizeof(correct_lines)/sizeof(correct_lines[0]); ++i)
    {
        libed2k::error_code ec;
        libed2k::datline2filter(correct_lines[i], ec);
        BOOST_CHECK(!ec);
    }

    const char* comm_lines[] =
    {
            "#",
            "",
            "#####",
            "#,,,",
            "#"
    };

    for(size_t i = 0; i < sizeof(comm_lines)/sizeof(comm_lines[0]); ++i)
    {
        libed2k::error_code ec;
        libed2k::datline2filter(comm_lines[i], ec);
        BOOST_CHECK(ec == libed2k::error_code(libed2k::errors::empty_or_commented_line));
    }

    const char* syntax_errors[] =
    {
            ",,,",
            "192.168.9.0 - 192.168.9.24 - , 10, XXX",
            "192.168.9.0 - 192.168.9.24 - 225.168.9.24, 10, XXX, YYY, ZZZ",
            "192.168.9.0 -, 10, XXX",
            "- 192.168.9.24 , 10, XXX",
            "192.168.9.0 - 192.168.9.24, 0000",
            "192.168.9.0, 10, XXX",
            "192.168.9.0 - 192.168.9.45, 10"
    };

    for(size_t i = 0; i < sizeof(syntax_errors)/sizeof(syntax_errors[0]); ++i)
    {
        libed2k::error_code ec;
        libed2k::datline2filter(syntax_errors[i], ec);
        BOOST_CHECK_MESSAGE(ec == libed2k::error_code(libed2k::errors::lines_syntax_error), syntax_errors[i]);
    }

    // full checks
    libed2k::error_code ec;
    libed2k::dat_rule f = libed2k::datline2filter("0.0.0.0         - 5.79.127.255    , 0   , Non-IS Net", ec);
    BOOST_REQUIRE(!ec);
    BOOST_CHECK_EQUAL(f.begin, libed2k::ip::address::from_string("0.0.0.0", ec));
    BOOST_REQUIRE(!ec);
    BOOST_CHECK_EQUAL(f.end, libed2k::ip::address::from_string("5.79.127.255", ec));
    BOOST_REQUIRE(!ec);
    BOOST_CHECK_EQUAL(f.level, 0);

    f = libed2k::datline2filter("31.208.0.0      - 37.139.255.255  , 56   , Non-IS Net", ec);
    BOOST_REQUIRE(!ec);
    BOOST_CHECK_EQUAL(f.begin, libed2k::ip::address::from_string("31.208.0.0", ec));
    BOOST_REQUIRE(!ec);
    BOOST_CHECK_EQUAL(f.end, libed2k::ip::address::from_string("37.139.255.255", ec));
    BOOST_REQUIRE(!ec);
    BOOST_CHECK_EQUAL(f.level, 56);

    // incorrect addresses
    ec = libed2k::error_code(libed2k::errors::no_error);
    libed2k::datline2filter("31.208.0.000      - 037.139.255.255  , 56   , Non-IS Net", ec);
#if defined __APPLE__ || defined WIN32
    BOOST_CHECK(!ec);
    BOOST_CHECK_EQUAL(f.begin, libed2k::ip::address::from_string("31.208.0.0", ec));
    BOOST_CHECK_EQUAL(f.end, libed2k::ip::address::from_string("37.139.255.255", ec));
    BOOST_CHECK_EQUAL(f.level, 56);
#else
    BOOST_CHECK(ec);
#endif
    ec = libed2k::error_code(libed2k::errors::no_error);
    libed2k::datline2filter("31.20A.0.0      - 37.139.255.255  , 56   , Non-IS Net", ec);
    BOOST_CHECK(ec);

    // incorrect levels
    ec = libed2k::error_code(libed2k::errors::no_error);
    libed2k::datline2filter("31.208.0.0      - 37.139.255.255  ,  A  , Non-IS Net", ec);
    BOOST_CHECK_EQUAL(ec, libed2k::error_code(libed2k::errors::levels_value_error));

    ec = libed2k::error_code(libed2k::errors::no_error);
    libed2k::datline2filter("31.208.0.0      - 37.139.255.255  ,  123A  , Non-IS Net", ec);
    BOOST_CHECK_EQUAL(ec, libed2k::error_code(libed2k::errors::levels_value_error));

    ec = libed2k::error_code(libed2k::errors::no_error);
    libed2k::datline2filter("31.208.0.0      - 37.139.255.255  , , Non-IS Net", ec);
    BOOST_CHECK_EQUAL(ec, libed2k::error_code(libed2k::errors::levels_value_error));
}

BOOST_AUTO_TEST_CASE(multipiece_request_test)
{
    std::vector<libed2k::peer_request> res1;
    res1.push_back(libed2k::peer_request(47, 9712413, 15587));
    res1.push_back(libed2k::peer_request(48, 0, 45853));
    BOOST_CHECK(mk_peer_requests(466928413, 466989853, 490106914) == res1);
}

BOOST_AUTO_TEST_SUITE_END()
