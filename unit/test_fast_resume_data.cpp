#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#include <string>

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <boost/test/unit_test.hpp>
#include "libed2k/constants.hpp"
#include "libed2k/types.hpp"
#include "libed2k/file.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/log.hpp"

namespace libed2k
{
    namespace aux
    {
        class session_fast_rd : public session_impl_base
        {
        public:
            session_fast_rd(const session_settings& settings);
            virtual transfer_handle add_transfer(add_transfer_params const&, error_code& ec);
            virtual boost::weak_ptr<transfer> find_transfer(const fs::path& path);
            virtual void remove_transfer(const transfer_handle& h, int options);
            void stop();
            void run();
        private:
            boost::mutex        m_mutex;
        };

        session_fast_rd::session_fast_rd(const session_settings& settings) : session_impl_base(settings)
        {
        }

        transfer_handle session_fast_rd::add_transfer(add_transfer_params const& t, error_code& ec)
        {
            boost::mutex::scoped_lock lock(m_mutex);
            DBG("addtransfer: " << libed2k::convert_to_native(libed2k::bom_filter(t.file_path.string()))
                << " collection: " << libed2k::convert_to_native(libed2k::bom_filter(t.m_collection_path.string())));
            return (transfer_handle(boost::weak_ptr<transfer>()));
        }

        boost::weak_ptr<transfer> session_fast_rd::find_transfer(const fs::path& path)
        {
            return boost::weak_ptr<transfer>();
        }

        void session_fast_rd::remove_transfer(const transfer_handle& h, int options)
        {

        }

        void session_fast_rd::stop()
        {

            DBG("fmon stop");
            m_file_hasher.stop();
            DBG("fmon stop complete");
        }

        void session_fast_rd::run()
        {
            m_io_service.run();
        }
    }
}

BOOST_AUTO_TEST_SUITE(test_fast_resume_data)

BOOST_AUTO_TEST_CASE(test_shared_files)
{
    libed2k::session_settings s;
    libed2k::aux::session_fast_rd session(s);
}

BOOST_AUTO_TEST_SUITE_END()
