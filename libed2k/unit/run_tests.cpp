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

BOOST_AUTO_TEST_CASE(test_rules_simple)
{
    libed2k::rule r(libed2k::rule::rt_plus, "string");
    BOOST_CHECK(r.add_sub_rule(libed2k::rule::rt_minus, "xxxx"));
    BOOST_CHECK(r.add_sub_rule(libed2k::rule::rt_minus, "yyyy"));
    BOOST_CHECK(r.add_sub_rule(libed2k::rule::rt_minus, "zzzz"));
    BOOST_CHECK(r.add_sub_rule(libed2k::rule::rt_minus, "xxxx") == NULL);
    BOOST_CHECK(r.add_sub_rule(libed2k::rule::rt_minus, "zzzz") == NULL);
    libed2k::rule* sr = r.add_sub_rule(libed2k::rule::rt_minus, "dir1");
    libed2k::rule* sr2 = sr->add_sub_rule(libed2k::rule::rt_plus, "dir2");

    std::string strName;
    strName = sr2->get_filename();
    const libed2k::rule* p = sr2->get_parent();

    while(p)
    {
        strName = p->get_filename() + std::string("-") + strName;
        p = p->get_parent();
    }

    BOOST_CHECK_EQUAL(strName, "string-dir1-dir2");

    BOOST_CHECK(r.match("string/xxxx"));
    BOOST_CHECK(r.match("string/yyyy"));
    BOOST_CHECK(r.match("string/zzzz"));
    BOOST_CHECK(r.match("string/xxxx4") == NULL);

}


BOOST_AUTO_TEST_SUITE_END()

