#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <fstream>
#include <boost/test/unit_test.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem/fstream.hpp>
#include <locale.h>
#include "libed2k/constants.hpp"
#include "libed2k/file.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/log.hpp"
#include "libed2k/transfer.hpp"
#include "libed2k/deadline_timer.hpp"

namespace libed2k
{
    namespace aux
    {
        class session_impl_test : public session_impl_base
        {
        public:
            session_impl_test(const session_settings& settings);
            virtual transfer_handle add_transfer(add_transfer_params const&, error_code& ec);
            virtual boost::weak_ptr<transfer> find_transfer(const fs::path& path);
            virtual void remove_transfer(const transfer_handle& h, int options);
            void stop();
            void wait();
            void run();
            void save();    // this method emulated save_state in session_base
            const std::deque<pending_collection>& get_pending_collections() const;
            void on_timer();
            const std::deque<add_transfer_params>& get_transfers_map() const;
            std::vector<transfer_handle> get_transfers();
            std::vector<md4_hash> get_hashes() const;

            bool                m_ready;
            int                 m_hash_count;
            boost::mutex        m_mutex;
            boost::condition    m_signal;
            std::vector<md4_hash> m_vH;
            std::deque<add_transfer_params> m_vParams;
            std::deque<boost::shared_ptr<transfer> > m_transfers;
            std::vector<peer_entry> m_peers;
            bool                m_bAfterSave;
            deadline_timer      m_timer;
        };

        session_impl_test::session_impl_test(const session_settings& settings) : session_impl_base(settings), m_ready(true), m_hash_count(0),
                m_timer(m_io_service,  libed2k::seconds(5))
        {
            m_bAfterSave = false;
            m_file_hasher.start();
            m_timer.async_wait(boost::bind(&session_impl_test::on_timer, this));
        }

        transfer_handle session_impl_test::add_transfer(add_transfer_params const& t, error_code& ec)
        {
            boost::mutex::scoped_lock lock(m_mutex);
            DBG("addtransfer: " << libed2k::convert_to_native(libed2k::bom_filter(t.file_path.string())) 
                << " collection: " << libed2k::convert_to_native(libed2k::bom_filter(t.collection_path.string())));
            // before save collect all transfers
            ++m_hash_count;
            update_pendings(t, false);
            m_vParams.push_back(t);
            // incorrect base pointer - using only for call constructor
            m_transfers.push_back(boost::shared_ptr<transfer>(new transfer(*((aux::session_impl*)this), m_peers,
                    t.file_hash,
                    t.file_path,
                    t.file_size)));
            m_timer.expires_from_now(libed2k::seconds(5));
            return (transfer_handle(boost::weak_ptr<transfer>()));
        }

        boost::weak_ptr<transfer> session_impl_test::find_transfer(const fs::path& path)
        {
            DBG("find transfer for " << convert_to_native(path.string()));

            for (std::deque<add_transfer_params>::iterator itr = m_vParams.begin(); itr != m_vParams.end(); ++itr)
            {
                DBG("scan for transfer: " << convert_to_native(itr->file_path.string()));

                // works only after save
                if (itr->file_path == path)
                {
                    if (m_bAfterSave)
                    {
                        ++m_hash_count;
                        m_vParams.erase(itr);
                        break;
                    }
                }
            }

            for (std::deque<boost::shared_ptr<transfer> >::iterator itr = m_transfers.begin(); itr != m_transfers.end(); ++itr)
            {
                if ((*itr)->filepath() == path)
                {
                    return *itr;
                }
            }

            // after save we must have transfers
            if (m_bAfterSave)
            {
                BOOST_CHECK(false);
            }

            return boost::weak_ptr<transfer>();
        }

        void session_impl_test::remove_transfer(const transfer_handle& h, int options)
        {
            boost::shared_ptr<transfer> tptr = h.m_transfer.lock();
            if (!tptr) return;

            for (std::deque<boost::shared_ptr<transfer> >::iterator itr = m_transfers.begin(); itr != m_transfers.end(); ++itr)
            {

                if ((*itr)->hash() == tptr->hash())
                {
                    m_transfers.erase(itr);
                    break;
                }
            }
        }

        void session_impl_test::stop()
        {

            DBG("fmon stop");
            m_file_hasher.stop();
            DBG("fmon stop complete");
        }

        void session_impl_test::wait()
        {
            boost::mutex::scoped_lock lock(m_mutex);
            m_signal.wait(lock);
        }

        void session_impl_test::run()
        {
            m_io_service.run();
        }

        void session_impl_test::save()
        {
            m_hash_count = 0;
            m_bAfterSave = true;
            DBG("session_impl::save_state()");
            known_file_collection kfc;

            std::deque<add_transfer_params>::iterator itr = m_vParams.begin();

            while(itr != m_vParams.end())
            {
                itr->dump();
                kfc.m_known_file_list.add(known_file_entry(itr->file_hash,
                        itr->piece_hashses,
                        itr->file_path,
                        itr->file_size,
                        itr->accepted,
                        itr->requested,
                        itr->transferred,
                        itr->priority));

                ++itr;
            }

            try
            {
                if (!m_settings.m_known_file.empty())
                {
                    DBG("save to " << convert_to_native(m_settings.m_known_file));
                    boost::filesystem::ofstream fstream(convert_to_native(m_settings.m_known_file), std::ios::binary);
                    libed2k::archive::ed2k_oarchive ofa(fstream);
                    ofa << kfc;
                }
            }
            catch(libed2k_exception&)
            {
                BOOST_REQUIRE(false);
            }

            // restart timer
            m_timer.expires_from_now(libed2k::seconds(5));
            m_timer.async_wait(boost::bind(&session_impl_test::on_timer, this));
            m_ready = true;
        }

        const std::deque<pending_collection>& session_impl_test::get_pending_collections() const
        {
            return m_pending_collections;
        }

        void session_impl_test::on_timer()
        {

            if (m_timer.expires_at() <= deadline_timer::traits_type::now())
            {
                DBG("Timer expired");
                m_timer.expires_at(max_time());
                m_ready = false;
                m_signal.notify_all();
                return;
            }

            // Put the actor back to sleep.
            m_timer.async_wait(boost::bind(&session_impl_test::on_timer, this));
        }

        const std::deque<add_transfer_params>& session_impl_test::get_transfers_map() const
        {
            return m_vParams;
        }

        std::vector<transfer_handle> session_impl_test::get_transfers()
        {
            std::vector<transfer_handle> vh;
            std::transform(m_transfers.begin(), m_transfers.end(), std::back_inserter(vh), boost::bind(&transfer::handle, _1));
            for (size_t n = 0; n < m_transfers.size(); ++n)
            {
                m_vH.push_back(m_transfers[n]->hash());
            }

            return (vh);
        }

        std::vector<md4_hash> session_impl_test::get_hashes() const
        {
            std::vector<md4_hash> vH;

            for (size_t n = 0; n < m_transfers.size(); ++n)
            {
                vH.push_back(m_transfers[n]->hash());
            }

            return vH;
        }

    }
}

BOOST_AUTO_TEST_SUITE(test_known_files)

const char chRussianDirectory[] = {'\xEF', '\xBB', '\xBF', '\xD1', '\x80', '\xD1', '\x83', '\xD1', '\x81', '\xD1', '\x81', '\xD0', '\xBA', '\xD0', '\xB0', '\xD1', '\x8F', '\x20', '\xD0', '\xB4', '\xD0', '\xB8', '\xD1', '\x80', '\xD0', '\xB5', '\xD0', '\xBA', '\xD1', '\x82', '\xD0', '\xBE', '\xD1', '\x80', '\xD0', '\xB8', '\xD1', '\x8F', '\x00' };
const char chRussianFilename[] = { '\xD1', '\x80', '\xD1', '\x83', '\xD1', '\x81', '\xD1', '\x81', '\xD0', '\xBA', '\xD0', '\xB8', '\xD0', '\xB9', '\x20', '\xD1', '\x84', '\xD0', '\xB0', '\xD0', '\xB9', '\xD0', '\xBB', '\x00' };


void generate_file(size_t nSize, const char* pchFilename)
{
    std::ofstream of(pchFilename, std::ios_base::binary | std::ios_base::out);

    if (of)
    {
        // generate small file
        for (size_t i = 0; i < nSize; i++)
        {
            of << 'X';
        }
    }
}

void create_directory_tree()
{
    std::string strDirectory = chRussianDirectory;
    std::string strFilename  = chRussianFilename;
    libed2k::fs::path p(libed2k::convert_to_native(libed2k::bom_filter(strDirectory)));
    libed2k::fs::create_directories(p);
    p /= libed2k::convert_to_native(libed2k::bom_filter(strDirectory));
    libed2k::fs::create_directories(p);

    libed2k::fs::path p1 = p / libed2k::convert_to_native(libed2k::bom_filter(strFilename + "01.txt"));
    libed2k::fs::path p2 = p / libed2k::convert_to_native(libed2k::bom_filter(strFilename + "02.txt"));
    libed2k::fs::path p3 = p / libed2k::convert_to_native(libed2k::bom_filter(strFilename + "03.txt"));
    libed2k::fs::path p4 = p / libed2k::convert_to_native(libed2k::bom_filter(strFilename + "04.txt"));
    libed2k::fs::path p5 = p / libed2k::convert_to_native(libed2k::bom_filter(strFilename + "05.txt"));

    generate_file(100, p1.string().c_str());
    generate_file(libed2k::PIECE_SIZE, p2.string().c_str());
    generate_file(libed2k::PIECE_SIZE+1, p3.string().c_str());
    generate_file(libed2k::PIECE_SIZE*4, p4.string().c_str());
    generate_file(libed2k::PIECE_SIZE+4566, p5.string().c_str());

    std::ofstream fstr(p.string().c_str());

}

void drop_directory_tree()
{
    std::string strDirectory = chRussianDirectory;
    libed2k::fs::path p(libed2k::convert_to_native(libed2k::bom_filter(strDirectory)));
    libed2k::fs::remove_all(p);
}

BOOST_AUTO_TEST_CASE(test_string_conversions)
{
    setlocale(LC_CTYPE, "");
    std::string strDirectory = chRussianDirectory;
    std::string strNative = libed2k::convert_to_native(libed2k::bom_filter(strDirectory));

    if (CHECK_BOM(strDirectory.size(), strDirectory))
    {
        BOOST_CHECK_EQUAL(strDirectory.substr(3), libed2k::convert_from_native(strNative));
    }
}

BOOST_AUTO_TEST_CASE(test_file_hasher)
{
    LOGGER_INIT(LOG_FILE)
    libed2k::session_settings s;
    s.m_fd_list.push_back(std::make_pair(std::string(chRussianDirectory), true));
    libed2k::aux::session_impl_test st(s);
    st.stop();
}

BOOST_AUTO_TEST_CASE(test_base_collection_extractor)
{
    std::pair<std::string, std::string> sp =
            libed2k::extract_base_collection(libed2k::fs::path("/home/apavlov"), libed2k::fs::path("/home/apavlov/data"));
    BOOST_CHECK_EQUAL(sp.first, "homeapavlov");
    BOOST_CHECK_EQUAL(sp.second, "data");

    std::pair<std::string, std::string> sp2 =
                libed2k::extract_base_collection(libed2k::fs::path("/home/apavlov"), libed2k::fs::path("/home/apavlov/data"));
    BOOST_CHECK_EQUAL(sp2.first, "homeapavlov");
    BOOST_CHECK_EQUAL(sp2.second, "data");

    std::pair<std::string, std::string> sp3 =
                libed2k::extract_base_collection(libed2k::fs::path("/home/apavlov"), libed2k::fs::path("/home/apavlov/data/game/thrones"));
    BOOST_CHECK_EQUAL(sp3.first, "homeapavlov");
    BOOST_CHECK_EQUAL(sp3.second, "data-game-thrones");

    std::pair<std::string, std::string> sp4 =
                libed2k::extract_base_collection(libed2k::fs::path("/home/apavlov/data/warlord"), libed2k::fs::path("/home/apavlov/data"));
    BOOST_CHECK(sp4.first.empty());
    BOOST_CHECK(sp4.second.empty());

#ifdef WIN32
    std::pair<std::string, std::string> sp5 =
                libed2k::extract_base_collection(libed2k::fs::path("C:\\share\\mule"), libed2k::fs::path("C:\\share\\mule\\games"));
    BOOST_CHECK_EQUAL(sp5.first, "Csharemule");
    BOOST_CHECK_EQUAL(sp5.second, "games");
#endif

    // remove / and .
    std::pair<std::string, std::string> sp6 =
                libed2k::extract_base_collection(libed2k::fs::path("/home/apavlov"), libed2k::fs::path("/home/apavlov/data/"));
    BOOST_CHECK_EQUAL(sp6.first, "homeapavlov");
    BOOST_CHECK_EQUAL(sp6.second, "data");

    // empty collection test
    std::pair<std::string, std::string> sp7 =
                libed2k::extract_base_collection(libed2k::fs::path("/home/apavlov/data"), libed2k::fs::path("/home/apavlov/data"));
    BOOST_CHECK_EQUAL(sp7.first, "homeapavlovdata");
    BOOST_CHECK(sp7.second.empty());

#ifdef WIN32
    std::pair<std::string, std::string> sp8 =
                libed2k::extract_base_collection(libed2k::fs::path("C:"), libed2k::fs::path("C:\\share\\mule\\games\\"));
    BOOST_CHECK_EQUAL(sp8.first, "C");
    BOOST_CHECK_EQUAL(sp8.second, "share-mule-games");
#endif

    // remove / and .
    std::pair<std::string, std::string> sp9 =
                libed2k::extract_base_collection(libed2k::fs::path("/home/apavlov/"), libed2k::fs::path("/home/apavlov/data/"));
    BOOST_CHECK(sp9.first.empty());
    BOOST_CHECK(sp9.second.empty());

}

BOOST_AUTO_TEST_CASE(test_share_file_share_dir)
{
    setlocale(LC_CTYPE, "");
    create_directory_tree();

    // create collections directory
    libed2k::fs::path collect_path = libed2k::fs::initial_path();
    collect_path /= "collections";

    libed2k::fs::create_directory(collect_path);
    BOOST_REQUIRE(libed2k::fs::exists(collect_path));

    // path was created by create_directory_tree
    libed2k::fs::path path = libed2k::fs::initial_path();
    path /= libed2k::convert_to_native(libed2k::bom_filter(chRussianDirectory));
    path /= libed2k::convert_to_native(libed2k::bom_filter(chRussianDirectory));
    BOOST_REQUIRE(libed2k::fs::exists(path));

    // add additional directories
    libed2k::fs::path p1 = path / "1";
    libed2k::fs::create_directory(p1);
    libed2k::fs::path p2 = p1 / "2";
    libed2k::fs::create_directory(p2);

    // test directory filter
    std::deque<libed2k::fs::path> ex_paths;
    std::deque<libed2k::fs::path> fpaths;
    //std::transform(excludes.begin(), excludes.end(), std::back_inserter(ex_paths), boost::bind(&string2path, strPath, _1));
    std::copy(libed2k::fs::directory_iterator(path), libed2k::fs::directory_iterator(), std::back_inserter(fpaths));
    fpaths.erase(std::remove_if(fpaths.begin(), fpaths.end(), !boost::bind(&libed2k::aux::paths_filter, ex_paths, _1)), fpaths.end());
    BOOST_REQUIRE_EQUAL(fpaths.size(), 5U);

    {
        std::ofstream of1((p1 / "file1.txt").string().c_str());

        if (of1)
        {
            of1 << "some text data";
        }

        BOOST_REQUIRE(libed2k::fs::exists((p1 / "file1.txt")));

        std::ofstream of2((p2 / "file1.txt").string().c_str());

        if (of2)
        {
            of2 << "some text data in directory 2";
        }

        BOOST_REQUIRE(libed2k::fs::exists((p2 / "file1.txt")));
    }

    std::string strRoot = libed2k::convert_from_native(libed2k::fs::initial_path().string());   //!< root path
    std::string strPath = libed2k::convert_from_native(path.string());                          //!< share path
    std::string strPath1 = libed2k::convert_from_native(p1.string());                          //!< share path p1
    std::string strPath2 = libed2k::convert_from_native(p2.string());                          //!< share path p2

    std::pair<std::string, std::string> sp =
                    libed2k::extract_base_collection(libed2k::fs::path(strRoot), libed2k::fs::path(strPath));

    // check we have correct root and share path
    BOOST_REQUIRE(!sp.first.empty());
    BOOST_REQUIRE(!sp.second.empty());

    libed2k::session_settings s;
    s.m_known_file = "known.met";
    s.m_collections_directory = collect_path.string();
    libed2k::aux::session_impl_test st(s);

    std::deque<std::string> v;
    st.share_dir(strRoot, strPath, v, false);
    st.share_dir(strRoot, strPath1, v, false);
    st.share_dir(strRoot, strPath2, v, false);

    while(st.m_ready)
    {
        st.m_io_service.run_one();
        st.m_io_service.reset();
    }

    BOOST_CHECK(st.get_pending_collections().size() == 0);
    BOOST_CHECK(st.get_transfers().size() == (6 + 2 + 2));

    st.save();
    st.share_dir(strRoot, strPath, v, false);
    st.share_dir(strRoot, strPath1, v, false);
    st.share_dir(strRoot, strPath2, v, false);

    while(st.m_ready)
    {
        st.m_io_service.run_one();
        st.m_io_service.reset();
    }

    // we have to find all transfers
    BOOST_CHECK(st.get_transfers().size() == (6 + 2 + 2));
    BOOST_CHECK(st.get_transfers_map().size() == 0U);    // we found all transfers

    st.save();
    st.share_dir(strRoot, strPath1, v, true);
    st.share_dir(strRoot, strPath2, v, true);

    while(st.m_ready)
    {
        st.m_io_service.run_one();
        st.m_io_service.reset();
    }

    // we have to find all transfers
    BOOST_CHECK(st.get_transfers().size() == (6));

    std::vector<libed2k::md4_hash> vth = st.get_hashes();

    // hashes files were stayed in transfers
    std::vector<libed2k::md4_hash> vH;
    vH.push_back(libed2k::md4_hash::fromString("1AA8AFE3018B38D9B4D880D0683CCEB5"));
    vH.push_back(libed2k::md4_hash::fromString("E76BADB8F958D7685B4549D874699EE9"));
    vH.push_back(libed2k::md4_hash::fromString("49EC2B5DEF507DEA73E106FEDB9697EE"));
    vH.push_back(libed2k::md4_hash::fromString("9385DCEF4CB89FD5A4334F5034C28893"));
    vH.push_back(libed2k::md4_hash::fromString("9C7F988154D2C9AF16D92661756CF6B2"));

    for (size_t i = 0; i < vth.size(); ++i)
    {        

        for (std::vector<libed2k::md4_hash>::iterator itr = vH.begin(); itr != vH.end(); ++itr)
        {
            if (*itr == vth[i])
            {
                vH.erase(itr);
                break;
            }
        }
    }

    BOOST_CHECK(vH.empty());

    drop_directory_tree();
    libed2k::fs::remove_all(collect_path);
}

BOOST_AUTO_TEST_SUITE_END()
