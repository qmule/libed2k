#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif
#define BOOST_TEST_MODULE main
#include <string>
#include <boost/test/unit_test.hpp>
#include "libed2k/error_code.hpp"
#include "libed2k/ctag.hpp"
#include "libed2k/file.hpp"


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

BOOST_AUTO_TEST_CASE(test_options_bit_field)
{

}


BOOST_AUTO_TEST_SUITE_END()

