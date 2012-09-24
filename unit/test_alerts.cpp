#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <boost/test/unit_test.hpp>
#include <boost/thread/thread.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "libed2k/alert.hpp"
#include "libed2k/alert_types.hpp"


BOOST_AUTO_TEST_SUITE(test_alerts)

std::auto_ptr<libed2k::alert> pop_alert(libed2k::alert_manager& al)
{
    if (al.pending())
    {
        return al.get();
    }

    return std::auto_ptr<libed2k::alert>(0);
}

bool bGlobal = false;

void empty(const boost::system::error_code& /*e*/)
{
    bGlobal = true;
}

BOOST_AUTO_TEST_CASE(test_alerts)
{
    libed2k::io_service io;
    boost::asio::deadline_timer tm(io, boost::posix_time::seconds(5));  // run timer for service work
    libed2k::alert_manager al(io);

    tm.async_wait(&empty);
    boost::thread t(boost::bind(&libed2k::io_service::run, &io));
    al.set_alert_mask(0);

    al.post_alert(libed2k::server_connection_initialized_alert(1,1,1));
    al.post_alert(libed2k::server_connection_initialized_alert(2,2,2));
    al.post_alert(libed2k::server_connection_initialized_alert(3,2,2));

    std::auto_ptr<libed2k::alert> a;

    a = pop_alert(al);
    unsigned int nCount = 0;

    while (a.get())
    {
        nCount++;
        BOOST_REQUIRE(dynamic_cast<libed2k::server_connection_initialized_alert*>(a.get()));   // we wait for server alert now
        BOOST_CHECK_EQUAL((dynamic_cast<libed2k::server_connection_initialized_alert*>(a.get()))->m_nClientId, nCount);
        a = pop_alert(al);
    }

    BOOST_CHECK_EQUAL(nCount, 3U);

    if (al.should_post<libed2k::server_connection_initialized_alert>())
    {
        al.post_alert(libed2k::server_connection_initialized_alert(300,23,66));
    }

    a = pop_alert(al);
    BOOST_CHECK(!a.get());

    al.set_alert_mask(libed2k::alert::status_notification);

    if (al.should_post<libed2k::server_connection_initialized_alert>())
    {
        al.post_alert(libed2k::server_connection_initialized_alert(90, 1, 66));
    }

    a = pop_alert(al);

    BOOST_REQUIRE(a.get());
    BOOST_REQUIRE(dynamic_cast<libed2k::server_connection_initialized_alert*>(a.get()));
    BOOST_CHECK_EQUAL(dynamic_cast<libed2k::server_connection_initialized_alert*>(a.get())->m_nClientId, 90U);

    t.join();

    BOOST_CHECK(bGlobal);
}

BOOST_AUTO_TEST_SUITE_END()
