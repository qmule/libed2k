
#ifndef WIN32
#include <sys/resource.h>
#endif

#include <algorithm>
#include <libtorrent/peer_connection.hpp>
#include <libtorrent/socket.hpp>

#include "libed2k/session.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/transfer_handle.hpp"
#include "libed2k/transfer.hpp"
#include "libed2k/peer_connection.hpp"
#include "libed2k/server_connection.hpp"
#include "libed2k/constants.hpp"
#include "libed2k/log.hpp"
#include "libed2k/alert_types.hpp"
#include "libed2k/file.hpp"
#include "libed2k/util.hpp"

using namespace libed2k;
using namespace libed2k::aux;

dictionary_entry::dictionary_entry(boost::uintmax_t nFilesize) : file_size(nFilesize),
        m_accepted(0),
        m_requested(0),
        m_transferred(0),
        m_priority(0)
{
}

dictionary_entry::dictionary_entry() :
        file_size(0),
        m_accepted(0),
        m_requested(0),
        m_transferred(0),
        m_priority(0)
{
}

session_impl_base::session_impl_base(const session_settings& settings) :
        m_io_service(),
        m_settings(settings),
        m_transfers(),
        m_fmon(boost::bind(&session_impl_base::post_transfer, this, _1))
{
}

session_impl_base::~session_impl_base()
{

}

void session_impl_base::post_transfer(add_transfer_params const& params)
{
    DBG("session_impl_base::post_transfer");
    error_code ec;
    m_io_service.post(boost::bind(&session_impl_base::add_transfer, this, params, ec));
}

// resolve reference to reference and overloading problems
bool dref_is_regular_file(fs::path p)
{
    return fs::is_regular_file(p);
}

void session_impl_base::save_state() const
{
    DBG("session_impl::save_state()");
    known_file_collection kfc;

    for (transfer_map::const_iterator i = m_transfers.begin(),
            end(m_transfers.end()); i != end; ++i)
    {
        try
        {
            kfc.m_known_file_list.add(known_file_entry(i->second->hash(),
                i->second->hashset().all_hashes(),
                i->second->filepath(),
                i->second->getFilesize(),
                i->second->getAcepted(),
                i->second->getResuested(),
                i->second->getTransferred(),
                i->second->getPriority()));
        }
        catch(fs::filesystem_error& fe)
        {
            // we can get error when call last_write_time - ignore it and don't write this item into file
            ERR("file system error on save_state: " << fe.what());
        }
    }

    if (!m_settings.m_known_file.empty())
    {
        fs::ofstream fstream(convert_to_native(m_settings.m_known_file), std::ios::binary);
        libed2k::archive::ed2k_oarchive ofa(fstream);
        ofa << kfc;
    }
}

void session_impl_base::load_state()
{
    DBG("session_impl_base::load_state()");

    std::map<std::string, size_t> known_items;  //!< temporary map for known file dictionary
    known_file_collection kfc;                  //!< structured data buffer
    error_code  ec;                             //!< checking error code

    if (!m_settings.m_known_file.empty())
    {
        fs::ifstream fstream(convert_to_native(m_settings.m_known_file), std::ios::binary);

        if (fstream)
        {

            libed2k::archive::ed2k_iarchive ifa(fstream);

            try
            {
                ifa >> kfc;

                // generate transfers
                for (size_t n = 0; n < kfc.m_known_file_list.m_collection.size(); n++)
                {

                    // file name from known.met in UTF-8
                    std::string strFilename = kfc.m_known_file_list.m_collection[n].m_list.getStringTagByNameId(FT_FILENAME);

                    if (strFilename.empty())
                    {
                        continue;
                    }

                    known_items.insert(std::make_pair(strFilename, n));
                    DBG("session_impl_base::load_state: known file item: " << convert_to_native(strFilename));
                }
            }
            catch(libed2k_exception& e)
            {
                // hide parse errors
                ERR("session_impl_base::load_state: parse error " << e.what());
            }
        }

    }

    // scan directories and files from settings
    for (session_settings::fd_list::iterator itr = m_settings.m_fd_list.begin(); itr != m_settings.m_fd_list.end(); ++itr)
    {
        DBG("session_impl_base::load_state: scan directory: " << convert_to_native(itr->first));
        fs::path p(convert_to_native(bom_filter(itr->first)));  // convert UTF-8 properties to native
        std::vector<fs::path> v;

        try
        {
            if (fs::exists(p))
            {                

                if (fs::is_directory(p))
                {
                    if (itr->second)
                    {
                        std::copy(fs::recursive_directory_iterator(p), fs::recursive_directory_iterator(), std::back_inserter(v));
                    }
                    else
                    {
                        std::copy(fs::directory_iterator(p), fs::directory_iterator(), std::back_inserter(v));
                    }

                    v.erase(std::remove_if(v.begin(), v.end(), std::not1(std::ptr_fun(dref_is_regular_file))), v.end());
                }
                else if (fs::is_regular_file(p))
                {
                    v.push_back(p);
                }
            }
        }
        catch(fs::filesystem_error& e)
        {
            ERR("file system error: " << e.what());
        }

        for (std::vector<fs::path>::iterator itr = v.begin(); itr != v.end(); ++itr)
        {
            // search file name in known.met content before use convert current file name from native to utf8
            std::map<std::string, size_t>::iterator m = known_items.find(convert_from_native(itr->leaf()));
            boost::uint32_t nLastChangeTime = fs::last_write_time(*itr);

            // check file already processed - save it in native codepage
            transfer_filename_map::iterator p_itr = m_transfers_filenames.find(std::make_pair(itr->leaf(), nLastChangeTime));

            if (p_itr != m_transfers_filenames.end())
            {
                // file already processed
                DBG("file already processed");
                continue;
            }

            if (m != known_items.end())
            {
                DBG("item found in catalog: fs ts: " << nLastChangeTime << " catalog: " << kfc.m_known_file_list.m_collection[m->second].m_nLastChanged);
            }

            if (m != known_items.end() && nLastChangeTime == kfc.m_known_file_list.m_collection[m->second].m_nLastChanged)
            {
                size_t n = m->second;
                DBG("hash count: " << kfc.m_known_file_list.m_collection[n].m_hash_list.m_collection.size());
                DBG("known file: " << (itr->leaf()));

                // generate transfer
                add_transfer_params atp;

                atp.file_path = convert_from_native(itr->string());    // use utf8 codepage
                atp.file_hash = kfc.m_known_file_list.m_collection[n].m_hFile;

                if (kfc.m_known_file_list.m_collection[n].m_hash_list.m_collection.empty())
                {
                    // when file contain only one hash - we save main hash directly into container
                    atp.piece_hash.append(kfc.m_known_file_list.m_collection[n].m_hFile);
                }
                else
                {
                    atp.piece_hash.all_hashes(
                            kfc.m_known_file_list.m_collection[n].m_hash_list.m_collection);
                }

                for (size_t j = 0; j < kfc.m_known_file_list.m_collection[n].m_list.count(); j++)
                {
                    const boost::shared_ptr<base_tag> p = kfc.m_known_file_list.m_collection[n].m_list[j];

                    switch(p->getNameId())
                    {
                        case FT_FILENAME:
                            // ignore this tag - already loaded, second tag we don't use
                            break;
                        case FT_FILESIZE:
                            atp.file_size = p->asInt();
                            break;
                        case FT_ATTRANSFERRED:
                            atp.m_transferred += p->asInt();
                            break;
                        case FT_ATTRANSFERREDHI:
                            atp.m_transferred += (p->asInt() << 32);
                            break;
                        case FT_ATREQUESTED:
                            atp.m_requested = p->asInt();
                            break;
                        case FT_ATACCEPTED:
                            atp.m_accepted = p->asInt();
                            break;
                        case FT_ULPRIORITY:
                            atp.m_priority = p->asInt();
                            break;
                        default:
                            // ignore unused tags like
                            // FT_PERMISSIONS
                            // FT_AICH_HASH:
                            // and all kad tags
                            break;

                    }
                }

                // add file to control map in native
                m_transfers_filenames.insert(std::make_pair(std::make_pair(itr->leaf(), nLastChangeTime), atp.file_hash));
                // add transfer
                add_transfer(atp, ec);

                if (ec.value() == errors::session_is_closing)
                {
                    DBG("session_impl_base::load_state: session was closed");
                    break;
                }

                continue;
            }

            //ok, need hash this file
            DBG("hash file: " << (itr->string()));

            // add file name to control map in native code page
            m_transfers_filenames.insert(std::make_pair(std::make_pair(itr->leaf(), nLastChangeTime), md4_hash(md4_hash::m_emptyMD4Hash)));            
            // go to hashing - send fs in native code page - will be converted to utf8 by monitor
            m_fmon.m_order.push(std::make_pair(std::string(""), *itr));
        }

        if (ec.value() == errors::session_is_closing)
        {
            DBG("session is closed, exit");
            break;
        }
    }

}

dictionary_entry session_impl_base::get_dictionary_entry(const fs::path& file)
{
    BOOST_ASSERT(fs::is_regular_file(file));

    dictionary_entry de;
    boost::uint32_t nChangeTS = fs::last_write_time(file);
    files_dictionary::iterator itr = m_dictionary.find(std::make_pair(nChangeTS, file.filename()));

    if (itr != m_dictionary.end())
    {
        de = itr->second;
        m_dictionary.erase(itr);
    }

    return (de);
}

void session_impl_base::share_files(rule* base_rule)
{
    BOOST_ASSERT(base_rule);

    error_code ec;
    // share single file
    if (fs::is_regular_file(base_rule->get_path()))
    {
        // first - search file in transfers
        if (boost::shared_ptr<transfer> p = find_transfer(base_rule->get_path()).lock())
        {
            // file already in transfer list - pass it
            return;
        }

        dictionary_entry de = get_dictionary_entry(base_rule->get_path());

        if (de.m_hash.defined())
        {
            // add transfer
            add_transfer_params atp;
            atp.file_size = de.file_size;
            atp.file_hash = de.m_hash;
            atp.file_path = base_rule->get_path();
            atp.piece_hash = de.piece_hash;
            add_transfer(atp, ec);
        }
        else
        {
            // async add transfer by file monitor
            m_fmon.m_order.push(std::make_pair(std::string(""), base_rule->get_path()));
        }
    }

    try
    {
        emule_collection ecoll;
        fs::path collection_path;
        std::deque<fs::path> fpaths;

        if (fs::exists(base_rule->get_path()))
        {
            std::copy(fs::directory_iterator(base_rule->get_path()), fs::directory_iterator(), std::back_inserter(fpaths));

            // generate collection name
            std::string strCollectionName;
            std::string strPrefix;

            if (base_rule->get_type() != rule::rt_minus && !fpaths.empty() && !m_settings.m_collections_directory.empty())
            {
                std::stringstream sstr;

                strCollectionName = base_rule->get_filename();
                const rule* p = base_rule->get_parent();

                while(p)
                {
                    strCollectionName = p->get_filename() + std::string("-") + strCollectionName;
                    strPrefix = p->get_directory_prefix();
                    p = p->get_parent();
                }

                sstr << strCollectionName << "-" << fpaths.size() << ".emulecollection";
            }

            // we have collection name - generate collection path
            if (!strCollectionName.empty())
            {
                collection_path = convert_to_native(bom_filter(m_settings.m_collections_directory));
                collection_path /= convert_to_native(bom_filter(strPrefix));
                collection_path /= convert_to_native(bom_filter(strCollectionName));
            }

            // prepare pending collection
            pending_collection pc(strCollectionName);

            for (std::deque<fs::path>::iterator itr = fpaths.begin(); itr != fpaths.end(); ++itr)
            {
                // process regular file
                if (fs::is_regular_file(*itr))
                {
                    if ((base_rule->match(*itr)) || (base_rule->get_type() != rule::rt_minus))
                    {
                        // first search file in transfers
                        if (boost::shared_ptr<transfer> p = find_transfer(base_rule->get_path()).lock())
                        {
                            pc.m_files.push_back(pending_file(*itr, p->hash()));
                            continue;
                        }

                        dictionary_entry de = get_dictionary_entry(*itr);

                        if (de.m_hash.defined())
                        {
                            // add file to collection and create transfer
                            add_transfer_params atp(strCollectionName);
                            atp.file_size = de.file_size;
                            atp.file_hash = de.m_hash;
                            atp.file_path = base_rule->get_path();
                            atp.piece_hash = de.piece_hash;
                            add_transfer(atp, ec);
                        }
                        else
                        {
                            // add file to collection and run file monitor
                            pc.m_files.push_back(pending_file(*itr, md4_hash()));
                            m_fmon.m_order.push(std::make_pair(strCollectionName, *itr));
                        }

                    }

                    continue;
                }

                // process directory
                if (fs::is_directory(*itr))
                {
                    if (rule* pr = base_rule->match(*itr))
                    {
                        // scan next level
                        share_files(pr);
                    }
                    else if (base_rule->get_type() == rule::rt_asterisk)
                    {
                        // recursive rule - generate new rule for this directory and run on it
                        rule* pr = base_rule->add_sub_rule(rule::rt_asterisk, itr->filename());
                        share_files(pr);
                    }
                }
            }

            // we have collection - begin process
            if (!strCollectionName.empty())
            {
                // when we collect pending collection - add it to pending list for public after hashes were completed
                if (pc.is_pending())
                {
                    // first - search file in transfers
                    if (boost::shared_ptr<transfer> p = find_transfer(collection_path).lock())
                    {
                        // we found transfer on pending collection - stop it
                        p->abort();
                    }

                    // simple remove item from dictionary if it exists
                    dictionary_entry de = get_dictionary_entry(collection_path);

                    // scan transfers
                    m_pending_collections.push_back(pc);
                }
                else
                {
                    emule_collection ecoll = emule_collection::fromFile(collection_path.string());

                    // we already have collection on disk
                    if (boost::shared_ptr<transfer> p = find_transfer(collection_path).lock())
                    {
                        // transfer exists but collection changed
                        if (ecoll == pc.m_files)
                        {
                        }
                        else
                        {
                            p->abort();
                            // save collection to disk
                            // run hash
                            m_fmon.m_order.push(std::make_pair(strCollectionName, collection_path));
                        }
                    }
                    else
                    {
                        // transfer not exists

                        if (ecoll == pc.m_files)
                        {
                            // add transfer for collection
                        }
                        else
                        {
                            // save collection and run hash
                            m_fmon.m_order.push(std::make_pair(strCollectionName, collection_path));
                        }
                    }
                }
            }
        }
    }
    catch(fs::filesystem_error& e)
    {
        ERR("file system error: " << e.what());
    }

}

void session_impl_base::load_dictionary()
{
    m_dictionary.clear();

    if (!m_settings.m_known_file.empty())
    {
        fs::ifstream fstream(convert_to_native(m_settings.m_known_file), std::ios::binary);

        if (fstream)
        {

            libed2k::archive::ed2k_iarchive ifa(fstream);

            try
            {
                known_file_collection kfc;                  //!< structured data buffer
                ifa >> kfc;

                // generate transfers
                for (size_t n = 0; n < kfc.m_known_file_list.m_collection.size(); n++)
                {
                    // generate transfer
                    dictionary_key key;
                    dictionary_entry entry;

                    key.first = kfc.m_known_file_list.m_collection[n].m_nLastChanged;
                    entry.m_hash = kfc.m_known_file_list.m_collection[n].m_hFile;

                    if (kfc.m_known_file_list.m_collection[n].m_hash_list.m_collection.empty())
                    {
                        // when file contain only one hash - we save main hash directly into container
                        entry.piece_hash.append(kfc.m_known_file_list.m_collection[n].m_hFile);
                    }
                    else
                    {
                        entry.piece_hash.all_hashes(
                                kfc.m_known_file_list.m_collection[n].m_hash_list.m_collection);
                    }

                    for (size_t j = 0; j < kfc.m_known_file_list.m_collection[n].m_list.count(); j++)
                    {
                        const boost::shared_ptr<base_tag> p = kfc.m_known_file_list.m_collection[n].m_list[j];

                        switch(p->getNameId())
                        {
                            case FT_FILENAME:
                            {
                                // take only first file name tag
                                if (key.second.empty())
                                {
                                    key.second = convert_to_native(bom_filter(p->asString()));  // dictionary contains names in native codepage
                                }

                                break;
                            }
                            case FT_FILESIZE:
                                entry.file_size = p->asInt();
                                break;
                            case FT_ATTRANSFERRED:
                                entry.m_transferred += p->asInt();
                                break;
                            case FT_ATTRANSFERREDHI:
                                entry.m_transferred += (p->asInt() << 32);
                                break;
                            case FT_ATREQUESTED:
                                entry.m_requested = p->asInt();
                                break;
                            case FT_ATACCEPTED:
                                entry.m_accepted = p->asInt();
                                break;
                            case FT_ULPRIORITY:
                                entry.m_priority = p->asInt();
                                break;
                            default:
                                // ignore unused tags like
                                // FT_PERMISSIONS
                                // FT_AICH_HASH:
                                // and all kad tags
                                break;
                        }
                    }

                    if (key.first != 0 && !key.second.empty())
                    {
                        // add new entry to dictionary
                        m_dictionary.insert(std::make_pair(key, entry));
                    }
                }
            }
            catch(libed2k_exception& e)
            {
                // hide parse errors
                ERR("session_impl_base::load_state: parse error " << e.what());
            }
        }
    }
}

session_impl::session_impl(const fingerprint& id, const char* listen_interface,
                           const session_settings& settings): session_impl_base(settings),
    m_peer_pool(500),
    m_send_buffers(send_buffer_size),
    m_filepool(40),
    m_alerts(m_io_service),
    m_disk_thread(m_io_service, boost::bind(&session_impl::on_disk_queue, this),
                  m_filepool, DISK_BLOCK_SIZE),   
    m_half_open(m_io_service),
    m_server_connection(new server_connection(*this)),
    m_next_connect_transfer(m_transfers),
    m_client_id(0),
    m_abort(false),
    m_paused(false),
    m_max_connections(200),
    m_last_second_tick(time_now()),
    m_timer(m_io_service),
    m_reconnect_counter(-1)
{
    DBG("*** create ed2k session ***");

    error_code ec;
    m_listen_interface = tcp::endpoint(
        ip::address::from_string(listen_interface, ec), settings.listen_port);
    assert(!ec);

#ifdef WIN32
    // windows XP has a limit on the number of
    // simultaneous half-open TCP connections
    // here's a table:

    // windows version       half-open connections limit
    // --------------------- ---------------------------
    // XP sp1 and earlier    infinite
    // earlier than vista    8
    // vista sp1 and earlier 5
    // vista sp2 and later   infinite

    // windows release                     version number
    // ----------------------------------- --------------
    // Windows 7                           6.1
    // Windows Server 2008 R2              6.1
    // Windows Server 2008                 6.0
    // Windows Vista                       6.0
    // Windows Server 2003 R2              5.2
    // Windows Home Server                 5.2
    // Windows Server 2003                 5.2
    // Windows XP Professional x64 Edition 5.2
    // Windows XP                          5.1
    // Windows 2000                        5.0

    OSVERSIONINFOEX osv;
    memset(&osv, 0, sizeof(osv));
    osv.dwOSVersionInfoSize = sizeof(osv);
    GetVersionEx((OSVERSIONINFO*)&osv);

    // the low two bytes of windows_version is the actual
    // version.
    boost::uint32_t windows_version
        = ((osv.dwMajorVersion & 0xff) << 16)
        | ((osv.dwMinorVersion & 0xff) << 8)
        | (osv.wServicePackMajor & 0xff);

    // this is the format of windows_version
    // xx xx xx
    // |  |  |
    // |  |  + service pack version
    // |  + minor version
    // + major version

    // the least significant byte is the major version
    // and the most significant one is the minor version
    if (windows_version >= 0x060100)
    {
        // windows 7 and up doesn't have a half-open limit
        m_half_open.limit(0);
    }
    else if (windows_version >= 0x060002)
    {
        // on vista SP 2 and up, there's no limit
        m_half_open.limit(0);
    }
    else if (windows_version >= 0x060000)
    {
        // on vista the limit is 5 (in home edition)
        m_half_open.limit(4);
    }
    else if (windows_version >= 0x050102)
    {
        // on XP SP2 the limit is 10
        m_half_open.limit(9);
    }
    else
    {
        // before XP SP2, there was no limit
        m_half_open.limit(0);
    }
#endif

#if defined TORRENT_BSD || defined TORRENT_LINUX
    // ---- auto-cap open files ----

    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
    {
        DBG("max number of open files: " << rl.rlim_cur);

        // deduct some margin for epoll/kqueue, log files,
        // futexes, shared objects etc.
        rl.rlim_cur -= 20;

        // 80% of the available file descriptors should go
        m_max_connections = (std::min)(m_max_connections, int(rl.rlim_cur * 8 / 10));
        // 20% goes towards regular files
        m_filepool.resize((std::min)(m_filepool.size_limit(), int(rl.rlim_cur * 2 / 10)));

        DBG("max connections: " << m_max_connections);
        DBG("max files: " << m_filepool.size_limit());
    }
#endif // TORRENT_BSD || TORRENT_LINUX

    m_io_service.post(boost::bind(&session_impl::on_tick, this, ec));

    m_thread.reset(new boost::thread(boost::ref(*this)));
}

session_impl::~session_impl()
{
    boost::mutex::scoped_lock l(m_mutex);

    DBG("*** shutting down session ***");
    abort();

    l.unlock();
    // we need to wait for the disk-io thread to
    // die first, to make sure it won't post any
    // more messages to the io_service containing references
    // to disk_io_pool inside the disk_io_thread. Once
    // the main thread has handled all the outstanding requests
    // we know it's safe to destruct the disk thread.
    DBG("waiting for disk io thread");
    m_disk_thread.join();

    DBG("waiting for main thread");
    m_thread->join();

    DBG("shutdown complete!");
}

void session_impl::operator()()
{
    // main session thread

    eh_initializer();

    if (m_listen_interface.port() != 0)
    {
        boost::mutex::scoped_lock l(m_mutex);
        open_listen_port();
    }

    m_server_connection->start();

    bool stop_loop = false;
    while (!stop_loop)
    {
        error_code ec;
        m_io_service.run(ec);
        if (ec)
        {
            ERR("session_impl::operator()" << ec.message());
            std::string err = ec.message();
        }
        m_io_service.reset();

        boost::mutex::scoped_lock l(m_mutex);
        stop_loop = m_abort;
    }

    boost::mutex::scoped_lock l(m_mutex);
    m_transfers.clear();
}

void session_impl::open_listen_port()
{
    // close the open listen sockets
    DBG("session_impl::open_listen_port()");
    m_listen_sockets.clear();

    // we should only open a single listen socket, that
    // binds to the given interface
    listen_socket_t s = setup_listener(m_listen_interface);

    if (s.sock)
    {
        m_listen_sockets.push_back(s);
        async_accept(s.sock);
    }
}

void session_impl::update_disk_thread_settings()
{
    disk_io_job j;
    j.buffer = (char*)&m_disk_thread_settings;
    j.action = disk_io_job::update_settings;
    m_disk_thread.add_job(j);
}

void session_impl::async_accept(boost::shared_ptr<ip::tcp::acceptor> const& listener)
{
    boost::shared_ptr<tcp::socket> c(new tcp::socket(m_io_service));
    listener->async_accept(
        *c, bind(&session_impl::on_accept_connection, this, c,
                 boost::weak_ptr<tcp::acceptor>(listener), _1));
}

void session_impl::on_accept_connection(boost::shared_ptr<tcp::socket> const& s,
                                        boost::weak_ptr<ip::tcp::acceptor> listen_socket,
                                        error_code const& e)
{
    boost::shared_ptr<tcp::acceptor> listener = listen_socket.lock();
    if (!listener) return;

    if (e == boost::asio::error::operation_aborted)
    {
        s->close();
        DBG("session_impl::on_accept_connection: abort operation" );
        return;
    }

    if (m_abort)
    {
        DBG("session_impl::on_accept_connection: abort set");
        return;
    }

    error_code ec;
    if (e)
    {
        tcp::endpoint ep = listener->local_endpoint(ec);

        std::string msg =
            "error accepting connection on '" +
            libtorrent::print_endpoint(ep) + "' " + e.message();
        DBG(msg);

#ifdef TORRENT_WINDOWS
        // Windows sometimes generates this error. It seems to be
        // non-fatal and we have to do another async_accept.
        if (e.value() == ERROR_SEM_TIMEOUT)
        {
            async_accept(listener);
            return;
        }
#endif
#ifdef TORRENT_BSD
        // Leopard sometimes generates an "invalid argument" error. It seems to be
        // non-fatal and we have to do another async_accept.
        if (e.value() == EINVAL)
        {
            async_accept(listener);
            return;
        }
#endif
        if (m_alerts.should_post<mule_listen_failed_alert>())
            m_alerts.post_alert(mule_listen_failed_alert(ep, e));
        return;
    }

    async_accept(listener);
    incoming_connection(s);
}

void session_impl::incoming_connection(boost::shared_ptr<tcp::socket> const& s)
{
    error_code ec;
    // we got a connection request!
    tcp::endpoint endp = s->remote_endpoint(ec);

    if (ec)
    {
        ERR(endp << " <== INCOMING CONNECTION FAILED, could not retrieve remote endpoint " << ec.message());
        return;
    }

    DBG("<== INCOMING CONNECTION " << endp);

    // don't allow more connections than the max setting
    if (num_connections() >= max_connections())
    {
        //TODO: fire alert here

        DBG("number of connections limit exceeded (conns: "
              << num_connections() << ", limit: " << max_connections()
              << "), connection rejected");

        return;
    }

    setup_socket_buffers(*s);

    boost::intrusive_ptr<peer_connection> c(new peer_connection(*this, s, endp, NULL));

    if (!c->is_disconnecting())
    {
        // store connection in map only for real peers
        if (m_server_connection->m_target.address() != endp.address())
        {
            m_connections.insert(c);
        }

        c->start();
    }
}

boost::weak_ptr<transfer> session_impl::find_transfer(const md4_hash& hash)
{
    transfer_map::iterator i = m_transfers.find(hash);

    if (i != m_transfers.end()) return i->second;

    return boost::weak_ptr<transfer>();
}

boost::weak_ptr<transfer> session_impl::find_transfer(const fs::path& path)
{
    transfer_map::iterator itr = m_transfers.begin();

    while(itr != m_transfers.end())
    {
        if (itr->second->filepath() == path)
        {
            return itr->second;
        }

        ++itr;
    }

    return (boost::weak_ptr<transfer>());
}

boost::intrusive_ptr<peer_connection> session_impl::find_peer_connection(const net_identifier& np) const
{
    connection_map::const_iterator itr = std::find_if(m_connections.begin(), m_connections.end(), boost::bind(&peer_connection::has_network_point, _1, np));
    if (itr != m_connections.end())  {  return *itr; }
    return boost::intrusive_ptr<peer_connection>();
}

boost::intrusive_ptr<peer_connection> session_impl::find_peer_connection(const md4_hash& hash) const
{
    connection_map::const_iterator itr =
            std::find_if(m_connections.begin(), m_connections.end(), boost::bind(&peer_connection::has_hash, _1, hash));
    if (itr != m_connections.end())  {  return *itr; }
    return boost::intrusive_ptr<peer_connection>();
}

transfer_handle session_impl::find_transfer_handle(const md4_hash& hash)
{
    return transfer_handle(find_transfer(hash));
}

peer_connection_handle session_impl::find_peer_connection_handle(const net_identifier& np)
{
    return peer_connection_handle(find_peer_connection(np), this);
}

peer_connection_handle session_impl::find_peer_connection_handle(const md4_hash& hash)
{
    return peer_connection_handle(find_peer_connection(hash), this);
}

std::vector<transfer_handle> session_impl::get_transfers()
{
    std::vector<transfer_handle> ret;

    for (session_impl::transfer_map::iterator i
        = m_transfers.begin(), end(m_transfers.end());
        i != end; ++i)
    {
        if (i->second->is_aborted()) continue;
        ret.push_back(transfer_handle(i->second));
    }

    return ret;
}

void session_impl::close_connection(const peer_connection* p, const error_code& ec)
{
    DBG("session_impl::close_connection(CLOSING CONNECTION " << p->remote() << " : " << ec.message() << ")");
    connection_map::iterator i =
        std::find_if(m_connections.begin(), m_connections.end(),
                     boost::bind(&boost::intrusive_ptr<peer_connection>::get, _1) == p);
    if (i != m_connections.end()) m_connections.erase(i);
}

transfer_handle session_impl::add_transfer(
    add_transfer_params const& params, error_code& ec)
{
    APP("add transfer: {hash: " << params.file_hash << ", path: " << params.file_path
        << ", size: " << params.file_size << "}");

    if (is_aborted())
    {
        ec = errors::session_is_closing;
        return transfer_handle();
    }

    // is the transfer already active?
    boost::shared_ptr<transfer> transfer_ptr = find_transfer(params.file_hash).lock();
    if (transfer_ptr)
    {
        if (!params.duplicate_is_error) {
            DBG("return existing transfer with same hash");
            return transfer_handle(transfer_ptr);
        }

        DBG("return invalid transfer");
        ec = errors::duplicate_transfer;
        return transfer_handle();
    }

    int queue_pos = 0;
    for (transfer_map::const_iterator i = m_transfers.begin(),
             end(m_transfers.end()); i != end; ++i)
    {
        int pos = i->second->queue_position();
        if (pos >= queue_pos) queue_pos = pos + 1;
    }

    // check pending collections only when transfer has collection link
    if (!params.m_collection_name.empty())
    {
        for (std::deque<pending_collection>::iterator itr = m_pending_collections.begin();
                itr != m_pending_collections.end(); ++itr)
        {
            if (itr->m_strName == params.m_collection_name)             // find collection
            {
                if (itr->update(params.file_path, params.file_hash))    // update file in collection
                {
                    if (!itr->is_pending())                             // save collection it it changes status
                    {

                        m_pending_collections.erase(itr);               // remove collection from pending list
                    }

                    break;
                }
            }
        }
    }

    // generate utf8 transfer params before transfer will add
    //params.file_path = convert_from_native(params.file_path.string());

    transfer_ptr.reset(new transfer(*this, m_listen_interface, queue_pos, params));
    transfer_ptr->start();

    m_transfers.insert(std::make_pair(params.file_hash, transfer_ptr));

    transfer_handle handle(transfer_ptr);
    m_alerts.post_alert_should(added_transfer_alert(handle));

    return handle;
}

peer_connection_handle session_impl::add_peer_connection(net_identifier np, error_code& ec)
{
    DBG("session_impl::add_peer_connection");

    if (is_aborted())
    {
        ec = errors::session_is_closing;
        return peer_connection_handle();
    }

    boost::intrusive_ptr<peer_connection> ptr = find_peer_connection(np);

    // peer already connected
    if (ptr)
    {
        DBG("connection exists");
        // already exists
        return peer_connection_handle(ptr, this);
    }

    tcp::endpoint endp(boost::asio::ip::address::from_string(int2ipstr(np.m_nIP)), np.m_nPort);
    boost::shared_ptr<tcp::socket> sock(new tcp::socket(m_io_service));
    setup_socket_buffers(*sock);

    boost::intrusive_ptr<peer_connection> c(
        new peer_connection(*this, boost::weak_ptr<transfer>(), sock, endp, NULL));

    m_connections.insert(c);

    m_half_open.enqueue(boost::bind(&peer_connection::connect, c, _1),
                        boost::bind(&peer_connection::on_timeout, c),
                        libtorrent::seconds(m_settings.peer_connect_timeout));

    return (peer_connection_handle(c, this));
}

std::vector<transfer_handle> session_impl::add_transfer_dir(
    const fs::path& dir, error_code& ec)
{
    DBG("using transfer dir: " << dir);
    std::vector<transfer_handle> handles;

    for (fs::recursive_directory_iterator i(dir), end; i != end; ++i)
    {
        if (fs::is_regular_file(i->path()))
        {
            known_file kfile(i->path().string());
            kfile.init();
            add_transfer_params params;
            params.file_hash = kfile.getFileHash();
            params.piece_hash.all_hashes(kfile.getPieceHashes());
            params.file_path = i->path();
            params.file_size = fs::file_size(i->path());
            params.seed_mode = true;
            transfer_handle handle = add_transfer(params, ec);
            if (ec) break;
            handles.push_back(handle);
        }
    }
    return handles;
}

std::pair<char*, int> session_impl::allocate_buffer(int size)
{
    int num_buffers = (size + send_buffer_size - 1) / send_buffer_size;

    boost::mutex::scoped_lock l(m_send_buffer_mutex);

    return std::make_pair((char*)m_send_buffers.ordered_malloc(num_buffers),
                          num_buffers * send_buffer_size);
}

void session_impl::free_buffer(char* buf, int size)
{
    int num_buffers = size / send_buffer_size;

    boost::mutex::scoped_lock l(m_send_buffer_mutex);
    m_send_buffers.ordered_free(buf, num_buffers);
}

char* session_impl::allocate_disk_buffer(char const* category)
{
    return m_disk_thread.allocate_buffer(category);
}

void session_impl::free_disk_buffer(char* buf)
{
    m_disk_thread.free_buffer(buf);
}

unsigned short session_impl::listen_port() const
{
    if (m_listen_sockets.empty()) return 0;
    return m_listen_sockets.front().external_port;
}

const tcp::endpoint& session_impl::server() const
{
    return m_server_connection->m_target;
}

void session_impl::abort()
{
    if (m_abort) return;
    DBG("*** ABORT CALLED ***");

    // abort the main thread
    m_abort = true;
    error_code ec;
    m_timer.cancel(ec);


    // close file monitor
    m_fmon.stop();

    // close the listen sockets
    for (std::list<listen_socket_t>::iterator i = m_listen_sockets.begin(),
             end(m_listen_sockets.end()); i != end; ++i)
    {
        DBG("session_impl::abort: close listen socket" );
        i->sock->close(ec);
    }

    DBG("aborting all transfers (" << m_transfers.size() << ")");
    // abort all transfers
    for (transfer_map::iterator i = m_transfers.begin(),
             end(m_transfers.end()); i != end; ++i)
    {
        i->second->abort();
    }

    DBG("aborting all server requests");
    //m_server_connection.abort_all_requests();
    m_server_connection->close(errors::session_is_closing);

    for (transfer_map::iterator i = m_transfers.begin();
         i != m_transfers.end(); ++i)
    {
        transfer& t = *i->second;
        t.abort();
    }

    DBG("aborting all connections (" << m_connections.size() << ")");

    // closing all the connections needs to be done from a callback,
    // when the session mutex is not held
    m_io_service.post(boost::bind(&libtorrent::connection_queue::close, &m_half_open));

    DBG("connection queue: " << m_half_open.size());
    DBG("without transfers connections size: " << m_connections.size());

    // abort all connections
    while (!m_connections.empty())
    {
        (*m_connections.begin())->disconnect(errors::stopping_transfer);
    }

    DBG("connection queue: " << m_half_open.size());
}

void session_impl::on_disk_queue()
{
}

void session_impl::on_tick(error_code const& e)
{
    boost::mutex::scoped_lock l(m_mutex);

    if (m_abort) return;

    if (e == boost::asio::error::operation_aborted) return;

    if (e)
    {
        ERR("*** TICK TIMER FAILED " << e.message());
        ::abort();
        return;
    }

    error_code ec;
    ptime now = time_now();
    m_timer.expires_from_now(time::milliseconds(100), ec);
    m_timer.async_wait(bind(&session_impl::on_tick, this, _1));

    // only tick the following once per second
    if (now - m_last_second_tick < time::seconds(1)) return;
    m_last_second_tick = now;

    // --------------------------------------------------------------
    // check for incoming connections that might have timed out
    // --------------------------------------------------------------
    // TODO: should it be implemented?

    // --------------------------------------------------------------
    // second_tick every transfer
    // --------------------------------------------------------------
    for (transfer_map::iterator i = m_transfers.begin(); i != m_transfers.end(); ++i)
    {
        transfer& t = *i->second;
        t.second_tick();
    }

    connect_new_peers();

    // --------------------------------------------------------------
    // disconnect peers when we have too many
    // --------------------------------------------------------------
    // TODO: should it be implemented?


    // --------------------------------------------------------------
    // check server connection
    // --------------------------------------------------------------

    if (m_reconnect_counter == 0)
    {
        DBG("session_impl::on_tick: reconnect server connection");
        // execute server connection restart
        if (!m_server_connection->online() && !m_server_connection->connecting())
        {
            m_server_connection->start();
        }
    }

    // decrement counter while it great than zero
    if (m_reconnect_counter >= 0)
    {
        --m_reconnect_counter;
    }

}

void session_impl::connect_new_peers()
{
    // TODO:
    // this loop will "hand out" max(connection_speed, half_open.free_slots())
    // to the transfers, in a round robin fashion, so that every transfer is
    // equally likely to connect to a peer

    int free_slots = m_half_open.free_slots();
    if (!m_transfers.empty() && free_slots > -m_half_open.limit() &&
        num_connections() < m_max_connections && !m_abort)
    {
        // this is the maximum number of connections we will
        // attempt this tick
        int max_connections_per_second = 10;
        int steps_since_last_connect = 0;
        int num_transfers = int(m_transfers.size());
        for (;;)
        {
            transfer& t = *m_next_connect_transfer->second;
            if (t.want_more_connections())
            {
                try
                {
                    if (t.try_connect_peer())
                    {
                        --max_connections_per_second;
                        --free_slots;
                        steps_since_last_connect = 0;
                    }
                }
                catch (std::bad_alloc&)
                {
                    // we ran out of memory trying to connect to a peer
                    // lower the global limit to the number of peers
                    // we already have
                    m_max_connections = num_connections();
                    if (m_max_connections < 2) m_max_connections = 2;
                }
            }

            ++m_next_connect_transfer;
            ++steps_since_last_connect;

            // if we have gone two whole loops without
            // handing out a single connection, break
            if (steps_since_last_connect > num_transfers * 2) break;
            // if there are no more free connection slots, abort
            if (free_slots <= -m_half_open.limit()) break;
            // if we should not make any more connections
            // attempts this tick, abort
            if (max_connections_per_second == 0) break;
            // maintain the global limit on number of connections
            if (num_connections() >= m_max_connections) break;
        }
    }
}

bool session_impl::has_active_transfer() const
{
    for (transfer_map::const_iterator i = m_transfers.begin(), end(m_transfers.end());
         i != end; ++i)
    {
        if (!i->second->is_paused())
        {
            return true;
        }
    }

    return false;
}

void session_impl::setup_socket_buffers(ip::tcp::socket& s)
{
    error_code ec;
    if (m_settings.send_socket_buffer_size)
    {
        tcp::socket::send_buffer_size option(m_settings.send_socket_buffer_size);
        s.set_option(option, ec);
    }
    if (m_settings.recv_socket_buffer_size)
    {
        tcp::socket::receive_buffer_size option(m_settings.recv_socket_buffer_size);
        s.set_option(option, ec);
    }
}

session_impl::listen_socket_t session_impl::setup_listener(
    ip::tcp::endpoint ep, bool v6_only)
{
    DBG("session_impl::setup_listener");
    error_code ec;
    listen_socket_t s;
    s.sock.reset(new tcp::acceptor(m_io_service));
    s.sock->open(ep.protocol(), ec);

    if (ec)
    {
        //ERR("failed to open socket: " << libtorrent::print_endpoint(ep)
        //    << ": " << ec.message().c_str());
    }

    s.sock->bind(ep, ec);

    if (ec)
    {
        // post alert

        char msg[200];
        snprintf(msg, 200, "cannot bind to interface \"%s\": %s",
                 libtorrent::print_endpoint(ep).c_str(), ec.message().c_str());
        ERR(msg);

        return listen_socket_t();
    }

    s.external_port = s.sock->local_endpoint(ec).port();
    s.sock->listen(5, ec);

    if (ec)
    {
        // post alert

        char msg[200];
        snprintf(msg, 200, "cannot listen on interface \"%s\": %s",
                 libtorrent::print_endpoint(ep).c_str(), ec.message().c_str());
        ERR(msg);

        return listen_socket_t();
    }

    // post alert succeeded

    DBG("listening on: " << ep << " external port: " << s.external_port);

    return s;
}

std::auto_ptr<alert> session_impl::pop_alert()
{
    if (m_alerts.pending())
    {
        return m_alerts.get();
    }

    return std::auto_ptr<alert>(0);
}

void session_impl::set_alert_dispatch(boost::function<void(alert const&)> const& fun)
{
    m_alerts.set_dispatch_function(fun);
}

alert const* session_impl::wait_for_alert(time_duration max_wait)
{
    return m_alerts.wait_for_alert(max_wait);
}

void session_impl::post_search_request(search_request& ro)
{
    m_server_connection->post_search_request(ro);
}

void session_impl::post_search_more_result_request()
{
    m_server_connection->post_search_more_result_request();
}

void session_impl::post_sources_request(const md4_hash& hFile, boost::uint64_t nSize)
{
    m_server_connection->post_sources_request(hFile, nSize);
}

void session_impl::announce(shared_file_entry& entry)
{
    shared_files_list offer_list;
    offer_list.add(entry);
    m_server_connection->post_announce(offer_list);
}

shared_files_list session_impl::get_announces() const
{
    shared_files_list offer_list;

    for (transfer_map::const_iterator i = m_transfers.begin(); i != m_transfers.end(); ++i)
    {
        transfer& t = *i->second;
        if (t.is_finished())
            offer_list.add(t.getAnnounce());
    }

    return (offer_list);
}

void session_impl::set_alert_mask(boost::uint32_t m)
{
    m_alerts.set_alert_mask(m);
}

size_t session_impl::set_alert_queue_size_limit(size_t queue_size_limit_)
{
    return m_alerts.set_alert_queue_size_limit(queue_size_limit_);
}

void session_impl::server_ready(
    boost::uint32_t client_id,
    boost::uint32_t tcp_flags,
    boost::uint32_t aux_port)
{
    APP("server_ready: client_id=" << client_id);
    m_client_id = client_id;
    m_tcp_flags = tcp_flags;
    m_aux_port  = aux_port;
    m_alerts.post_alert_should(server_connection_initialized_alert(m_client_id, m_tcp_flags, aux_port));

     // test code - for offer tests
    /*
    shared_files_list olist;
    shared_file_entry sf;
    sf.m_hFile = md4_hash("49EC2B5DEF507DEA73E106FEDB9697EE");
    boost::uint32_t nFileSize = libed2k::PIECE_SIZE+1;

    sf.m_network_point.m_nIP     = client_id;
    sf.m_network_point.m_nPort   = settings().listen_port;
    sf.m_list.add_tag(libed2k::make_string_tag("file.txt", FT_FILENAME, true));
    sf.m_list.add_tag(libed2k::make_typed_tag(nFileSize, FT_FILESIZE, true));
    //sf.m_list.add_tag(libed2k::make_typed_tag(nFileSize, FT_FILESIZE_HI, false));

    std::string strED2KFileType(libed2k::GetED2KFileTypeSearchTerm(libed2k::GetED2KFileTypeID("file.txt")));

    DBG("ed2k file type: " << strED2KFileType);

    if (!strED2KFileType.empty())
    {
        sf.m_list.add_tag(make_string_tag(strED2KFileType, FT_FILETYPE, true));
    }

    olist.m_collection.push_back(sf);
    */
}

void session_impl::on_server_stopped()
{
    DBG("session_impl::on_server_stopped");
    m_client_id   = 0;
    m_tcp_flags   = 0;
    m_aux_port    = 0;

    // set reconnect counter
    if (m_settings.server_reconnect_timeout > -1)
    {
        m_reconnect_counter = (m_settings.server_reconnect_timeout); // we check it every 1 s
        DBG("session_impl::on_server_stopped(restart from " <<  m_reconnect_counter << ")");
    }
}

void session_impl::server_conn_start()
{
    m_server_connection->start();
}

void session_impl::server_conn_stop()
{
    m_server_connection->stop();
}
