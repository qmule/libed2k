#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#include <string>

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <fstream>
#include <boost/test/unit_test.hpp>
#include <libed2k/stat.hpp>
#include <libed2k/bencode.hpp>
#include "libed2k/types.hpp"
#include "libed2k/log.hpp"
#include "libed2k/constants.hpp"
#include "libed2k/transfer.hpp"
#include "libed2k/file.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/error_code.hpp"
#include "libed2k/alert_types.hpp"

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
            virtual std::vector<transfer_handle> get_transfers();

            transfer_handle transfer2handle(const boost::shared_ptr<transfer>& t)
            {
                return transfer_handle(t);
            }

            bool                m_bWaitTransfer;
        private:
            boost::mutex        m_mutex;
            std::vector<boost::shared_ptr<transfer> > m_transfers;
            tcp::endpoint       m_listen_interface;
        };

        session_fast_rd::session_fast_rd(const session_settings& settings) : session_impl_base(settings), m_bWaitTransfer(true)
        {
            m_file_hasher.start();
        }

        transfer_handle session_fast_rd::add_transfer(add_transfer_params const& params, error_code& ec)
        {
            boost::mutex::scoped_lock lock(m_mutex);
            DBG("addtransfer: " << libed2k::convert_to_native(libed2k::bom_filter(params.file_path.string()))
                << " collection: " << libed2k::convert_to_native(libed2k::bom_filter(params.collection_path.string())));
            m_bWaitTransfer = false;

            boost::shared_ptr<transfer> transfer_ptr(new transfer(*((session_impl*)this), m_listen_interface, 0, params));

            m_transfers.push_back(transfer_ptr);
            return (transfer_handle(transfer_ptr));
        }

        std::vector<transfer_handle> session_fast_rd::get_transfers()
        {
            std::vector<transfer_handle> vt;
            std::transform(m_transfers.begin(), m_transfers.end(), std::back_inserter(vt), boost::bind(&session_fast_rd::transfer2handle, this, _1));
            return vt;
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

class test_file
{
public:
    test_file(const char* pchFilename, size_t nFilesize);
    ~test_file();
private:
    std::string m_strFilename;
};

test_file::test_file(const char* pchFilename, size_t nFilesize)
{
    std::ofstream of(pchFilename, std::ios_base::binary | std::ios_base::out);

    if (of)
    {
        // generate small file
        for (size_t i = 0; i < nFilesize; i++)
        {
            of << 'X';
        }

        m_strFilename = pchFilename;
    }
}

test_file::~test_file()
{
    if (!m_strFilename.empty() && libed2k::fs::exists(libed2k::fs::path(m_strFilename.c_str())))
    {
        libed2k::fs::remove(libed2k::fs::path(m_strFilename.c_str()));
    }
}

BOOST_AUTO_TEST_CASE(test_shared_files)
{

    libed2k::fs::path fast_file = libed2k::fs::initial_path();
    fast_file /= "fastfile.dmp";
    test_file tf(fast_file.string().c_str(), libed2k::PIECE_SIZE + 1092);

    libed2k::session_settings s;
    libed2k::aux::session_fast_rd session(s);
    session.share_file(libed2k::convert_from_native(fast_file.string()), false);

    while(session.m_bWaitTransfer)
    {
        session.m_io_service.run_one();
        session.m_io_service.reset();
    }

    std::vector<libed2k::transfer_handle> vt = session.get_transfers();
    BOOST_CHECK_EQUAL(vt.size(), 1U);
    return;

    int num_resume_data = 0;

    for (std::vector<libed2k::transfer_handle>::iterator i = vt.begin(); i != vt.end(); ++i)
    {
        libed2k::transfer_handle h = *i;
        if (!h.is_valid()) continue;
        try
        {

          if (h.status().state == libed2k::transfer_status::checking_files ||
              h.status().state == libed2k::transfer_status::queued_for_checking) continue;

          h.save_resume_data();
          ++num_resume_data;
        }
        catch(libed2k::libed2k_exception&)
        {}
    }

    while (num_resume_data > 0)
    {
        libed2k::alert const* a = session.wait_for_alert(libed2k::seconds(30));

        if (a == 0)
        {
            DBG(" aborting with " << num_resume_data << " outstanding torrents to save resume data for");
            break;
        }

        // Saving fastresume data can fail
        libed2k::save_resume_data_failed_alert const* rda = dynamic_cast<libed2k::save_resume_data_failed_alert const*>(a);

        if (rda)
        {
            --num_resume_data;
            session.pop_alert();
            try
            {
            // Remove torrent from session
            if (rda->m_handle.is_valid()) session.remove_transfer(rda->m_handle, 0);
            }
            catch(libed2k::libed2k_exception&)
            {}
            continue;
        }

        libed2k::save_resume_data_alert const* rd = dynamic_cast<libed2k::save_resume_data_alert const*>(a);

        if (!rd)
        {
            session.pop_alert();
            continue;
        }

        // Saving fast resume data was successful
        --num_resume_data;

        if (!rd->m_handle.is_valid()) continue;

        try
        {
          // Remove torrent from session
            session.remove_transfer(rd->m_handle, 0);
            session.pop_alert();
        }
        catch(libed2k::libed2k_exception&)
        {}
    }
}

BOOST_AUTO_TEST_CASE(test_entries_hash)
{
    libed2k::md4_hash h1 = libed2k::md4_hash::fromString("200102030405060708090A0B0C0D0F0D");
    libed2k::md4_hash h2 = libed2k::md4_hash::fromString("300102030475060708090A0B0C0D0F0D");
    libed2k::md4_hash h3 = libed2k::md4_hash::fromString("4A0112030405060708090A0B0C0D0F0D");
    int n1 = 4;
    int n2 = 6;
    int n3 = 7;

    libed2k::entry e(libed2k::entry::dictionary_t);
    // store current hashset
    e["hash-keys"]    = libed2k::entry::list_type();
    e["hash-values"]  = libed2k::entry::list_type();
    libed2k::entry::list_type& hk = e["hash-keys"].list();
    libed2k::entry::list_type& hv = e["hash-values"].list();

    hk.push_back(n1);
    hv.push_back(h1.toString());

    hk.push_back(n2);
    hv.push_back(h2.toString());

    hk.push_back(n3);
    hv.push_back(h3.toString());

    // ok, encode entry
    std::vector<char> container;
    libed2k::lazy_entry le;

    libed2k::bencode(std::back_inserter(container), e);
    BOOST_REQUIRE(!container.empty());
    BOOST_REQUIRE(libed2k::lazy_bdecode(&container[0], &container[0] + container.size(), le) == 0);
    BOOST_REQUIRE(le.type() == libed2k::lazy_entry::dict_t);

    const libed2k::lazy_entry* lek = le.dict_find_list("hash-keys");
    const libed2k::lazy_entry* lev = le.dict_find_list("hash-values");

    BOOST_REQUIRE(lek && lev);
    BOOST_REQUIRE_EQUAL(lek->list_size(), 3);
    BOOST_REQUIRE_EQUAL(lev->list_size(), 3);

    BOOST_CHECK(lek->list_at(0)->type() == libed2k::lazy_entry::int_t);
    BOOST_CHECK(lek->list_at(1)->type() == libed2k::lazy_entry::int_t);
    BOOST_CHECK(lek->list_at(2)->type() == libed2k::lazy_entry::int_t);

    BOOST_CHECK(lek->list_at(0)->int_value() == n1);
    BOOST_CHECK(lek->list_at(1)->int_value() == n2);
    BOOST_CHECK(lek->list_at(2)->int_value() == n3);

    BOOST_CHECK(lev->list_at(0)->type() == libed2k::lazy_entry::string_t);
    BOOST_CHECK(lev->list_at(1)->type() == libed2k::lazy_entry::string_t);
    BOOST_CHECK(lev->list_at(2)->type() == libed2k::lazy_entry::string_t);

    BOOST_CHECK(lev->list_at(0)->string_value() == h1.toString());
    BOOST_CHECK(lev->list_at(1)->string_value() == h2.toString());
    BOOST_CHECK(lev->list_at(2)->string_value() == h3.toString());

}

BOOST_AUTO_TEST_SUITE_END()
