#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <sstream>
#include <locale.h>
#include <boost/test/unit_test.hpp>

#include "libed2k/constants.hpp"
#include "libed2k/log.hpp"
#include "libed2k/alert.hpp"
#include "libed2k/session_impl.hpp"

namespace libed2k{

    class session_test : public aux::session_impl_base
    {
    public:
        session_test(const session_settings& settings) : aux::session_impl_base(settings){}
        virtual ~session_test(){}

        transfer_handle add_transfer(add_transfer_params const&, error_code& ec){return transfer_handle();}
        boost::weak_ptr<transfer> find_transfer(const std::string& filename) {return boost::shared_ptr<transfer>(); }
        void remove_transfer(const transfer_handle& h, int options) {}
        std::vector<transfer_handle> get_transfers() {return std::vector<transfer_handle>();}
    };
}

BOOST_AUTO_TEST_SUITE(test_session)

BOOST_AUTO_TEST_CASE(test_lowid_logic)
{
    libed2k::session_settings ss;
    libed2k::session_test ses(ss);
    BOOST_CHECK(ses.register_callback(101, libed2k::md4_hash::emule));
    BOOST_CHECK(!ses.register_callback(101, libed2k::md4_hash::terminal));   // do not erase old value in current logic
    BOOST_CHECK(ses.register_callback(102, libed2k::md4_hash::emule));
    BOOST_CHECK_EQUAL(ses.callbacked_lowid(101), libed2k::md4_hash::emule);
    BOOST_CHECK(ses.register_callback(101, libed2k::md4_hash::terminal));
    BOOST_CHECK_EQUAL(ses.callbacked_lowid(102), libed2k::md4_hash::emule);
    BOOST_CHECK_EQUAL(ses.callbacked_lowid(101), libed2k::md4_hash::terminal);
}

BOOST_AUTO_TEST_SUITE_END()
