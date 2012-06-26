#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#include <string>

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <boost/test/unit_test.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/stat.hpp>
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
                << " collection: " << libed2k::convert_to_native(libed2k::bom_filter(params.m_collection_path.string())));
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
    libed2k::rule root(libed2k::rule::rt_plus, fast_file.string());

    libed2k::session_settings s;
    libed2k::aux::session_fast_rd session(s);
    session.share_files(&root);

    while(session.m_bWaitTransfer)
    {
        session.m_io_service.run_one();
        session.m_io_service.reset();
    }

    std::vector<libed2k::transfer_handle> vt = session.get_transfers();
    BOOST_CHECK_EQUAL(vt.size(), 1);
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
        libed2k::alert const* a = session.wait_for_alert(boost::posix_time::seconds(30));

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
          // Remove old fastresume file if it exists
            /*
          vector<char> out;
          bencode(back_inserter(out), *rd->resume_data);
          const QString filepath = torrentBackup.absoluteFilePath(h.hash()+".fastresume");
          QFile resume_file(filepath);
          if (resume_file.exists())
            QFile::remove(filepath);
          if (!out.empty() && resume_file.open(QIODevice::WriteOnly)) {
            resume_file.write(&out[0], out.size());
            resume_file.close();
          }
          */
          // Remove torrent from session
            session.remove_transfer(rd->m_handle, 0);
            session.pop_alert();
        }
        catch(libed2k::libed2k_exception&)
        {}
    }
}

BOOST_AUTO_TEST_SUITE_END()
