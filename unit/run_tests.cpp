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
    libed2k::rule r(libed2k::rule::rt_plus, "./test_share");
    BOOST_CHECK(r.add_sub_rule(libed2k::rule::rt_minus, "pub1"));
    //BOOST_CHECK(r.add_sub_rule(libed2k::rule::rt_minus, "./test_share"));
   // BOOST_CHECK(r.add_sub_rule(libed2k::rule::rt_minus, "./test_share/pub3"));

    libed2k::rule* sr = r.add_sub_rule(libed2k::rule::rt_minus, "pub3");
    libed2k::rule* sr2 = sr->add_sub_rule(libed2k::rule::rt_plus, "pub4");

    std::string strName;
    strName = sr2->get_filename();
    const libed2k::rule* p = sr2->get_parent();

    while(p)
    {
        strName = p->get_filename() + std::string("-") + strName;
        p = p->get_parent();
    }

    BOOST_CHECK_EQUAL(strName, "test_share-pub3-pub4");

    //BOOST_CHECK(r.match("file11"));
    //BOOST_CHECK(r.match("pub"));
    //BOOST_CHECK(r.match("./test_share/pub3"));
    //BOOST_CHECK(!r.match("./test_share/pub5"));

}

BOOST_AUTO_TEST_CASE(test_rule_prefix)
{
    libed2k::rule r(libed2k::rule::rt_plus, "one/two/three");
    libed2k::rule r2(libed2k::rule::rt_plus, "d:\\one\\two/three\\");
    libed2k::rule r3(libed2k::rule::rt_plus, "D:\\one\\two/three\\/second///:\\");

    BOOST_CHECK_EQUAL(r.get_directory_prefix(), "onetwothree");
    BOOST_CHECK_EQUAL(r2.get_directory_prefix(), "donetwothree");
    BOOST_CHECK_EQUAL(r3.get_directory_prefix(), "Donetwothreesecond");
}


BOOST_AUTO_TEST_CASE(test_rule_recursion)
{
    libed2k::rule r(libed2k::rule::rt_plus, "/tmp");
    libed2k::rule* p = r.add_sub_rule(libed2k::rule::rt_minus, "pub1")->add_sub_rule(libed2k::rule::rt_plus, "pub2");
    std::pair<std::string, std::string> pres = p->generate_recursive_data();
    BOOST_CHECK_EQUAL(pres.first, std::string("tmp-pub1-pub2"));    // recursive name
    BOOST_CHECK_EQUAL(pres.second, std::string("tmp"));             // base path
}

BOOST_AUTO_TEST_CASE(test_rule_matching)
{
    libed2k::fs::path root_path = libed2k::fs::initial_path();
    root_path /= "test_share";
    BOOST_REQUIRE(libed2k::fs::exists(root_path));
    libed2k::rule root(libed2k::rule::rt_plus, root_path.string());
    //root.add_sub_rule(libed2k::rule::rt_plus, "pub1");
    //root.add_sub_rule(libed2k::rule::rt_minus, "pub2");
    //root.add_sub_rule(libed2k::rule::rt_asterisk, "pub3");

    std::deque<libed2k::fs::path> fpaths;
    std::copy(libed2k::fs::directory_iterator(root_path), libed2k::fs::directory_iterator(), std::back_inserter(fpaths));
    BOOST_CHECK_EQUAL(fpaths.size(), 6);
    fpaths.erase(std::remove_if(fpaths.begin(), fpaths.end(), !boost::bind(&libed2k::rule::match, &root, _1)), fpaths.end());
    BOOST_REQUIRE_EQUAL(fpaths.size(), 3);
    libed2k::fs::path f1 = root_path;
    libed2k::fs::path f2 = root_path;
    libed2k::fs::path f3 = root_path;
    f1 /= "file.txt";
    f2 /= "file2.txt";
    f3 /= "file3.txt";
    BOOST_CHECK(std::find(fpaths.begin(), fpaths.end(), f1) != fpaths.end());
    BOOST_CHECK(std::find(fpaths.begin(), fpaths.end(), f2) != fpaths.end());
    BOOST_CHECK(std::find(fpaths.begin(), fpaths.end(), f3) != fpaths.end());    
}

BOOST_AUTO_TEST_CASE(div_ceil_test)
{
    BOOST_CHECK_EQUAL(libed2k::div_ceil(10,2), 5);
    BOOST_CHECK_EQUAL(libed2k::div_ceil(10,3), 4);
    BOOST_CHECK_EQUAL(libed2k::div_ceil(13,2), 7);
}

BOOST_AUTO_TEST_SUITE_END()

