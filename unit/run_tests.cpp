#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif
#define BOOST_TEST_MODULE main
#include <string>
#include <boost/test/unit_test.hpp>

#include "libed2k/error_code.hpp"
#include "libed2k/ctag.hpp"
#include "libed2k/file.hpp"
#include "libed2k/transfer_handle.hpp"
#include "libed2k/util.hpp"


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
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeID("ff.song.mP3"), libed2k::ED2KFT_AUDIO);
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeID("..dfdf..song.AVi"), libed2k::ED2KFT_VIDEO);
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeID("dff/..fdsong.sdsdmp3"), libed2k::ED2KFT_ANY);
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeID("dfdf.songmp3"), libed2k::ED2KFT_ANY);
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeID(".dddfdf.song.DOC"), libed2k::ED2KFT_DOCUMENT);

    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeSearchTerm(libed2k::GetED2KFileTypeID("xxx.mp3")), libed2k::ED2KFTSTR_AUDIO);
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeSearchTerm(libed2k::GetED2KFileTypeID("xxx.avi")), libed2k::ED2KFTSTR_VIDEO);
    BOOST_CHECK_EQUAL(libed2k::GetED2KFileTypeSearchTerm(libed2k::GetED2KFileTypeID("xxx.raR")), libed2k::ED2KFTSTR_PROGRAM);

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

BOOST_AUTO_TEST_SUITE_END()
