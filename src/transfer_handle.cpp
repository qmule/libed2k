#include "libed2k/transfer_handle.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/transfer.hpp"

using libed2k::aux::session_impl;


#define LIBED2K_FORWARD(call) \
    boost::shared_ptr<transfer> t = m_transfer.lock(); \
    if (!t) return; \
    session_impl::mutex_t::scoped_lock l(t->session().m_mutex); \
    t->call

#define LIBED2K_FORWARD_RETURN(call, def) \
    boost::shared_ptr<transfer> t = m_transfer.lock(); \
    if (!t) return def; \
    session_impl::mutex_t::scoped_lock l(t->session().m_mutex); \
    return t->call

#define LIBED2K_FORWARD_RETURN2(call, def) \
    boost::shared_ptr<transfer> t = m_transfer.lock(); \
    if (!t) return def; \
    session_impl::mutex_t::scoped_lock l(t->session().m_mutex); \
    t->call

#if 0

#define LIBED2K_FORWARD(call) \
    boost::shared_ptr<transfer> t = m_transfer.lock(); \
    if (!t) throw_invalid_handle(); \
    session_impl::mutex_t::scoped_lock l(t->session().m_mutex); \
    t->call

#define LIBED2K_FORWARD_RETURN(call, def) \
    boost::shared_ptr<transfer> t = m_transfer.lock(); \
    if (!t) throw_invalid_handle(); \
    session_impl::mutex_t::scoped_lock l(t->session().m_mutex); \
    return t->call

#define LIBED2K_FORWARD_RETURN2(call, def) \
    boost::shared_ptr<transfer> t = m_transfer.lock(); \
    if (!t) throw_invalid_handle(); \
    session_impl::mutex_t::scoped_lock l(t->session().m_mutex); \
    t->call

#endif

namespace libed2k
{

#ifndef BOOST_NO_EXCEPTIONS
    void throw_invalid_handle()
    {
        throw libed2k_exception(errors::invalid_transfer_handle);
    }
#endif

    bool transfer_handle::is_valid() const
    {
        return !m_transfer.expired();
    }

    md4_hash transfer_handle::hash() const
    {
        static const md4_hash empty_hash(md4_hash::terminal);
        LIBED2K_FORWARD_RETURN(hash(), empty_hash);
    }

    std::string transfer_handle::name() const
    {
        LIBED2K_FORWARD_RETURN(name(), std::string());
    }

    std::string transfer_handle::save_path() const
    {
        LIBED2K_FORWARD_RETURN(save_path(), std::string());
    }

    size_type transfer_handle::size() const
    {
        LIBED2K_FORWARD_RETURN(size(), 0);
    }

    add_transfer_params transfer_handle::params() const
    {
        LIBED2K_FORWARD_RETURN(params(), add_transfer_params());
    }

    bool transfer_handle::is_seed() const
    {
        LIBED2K_FORWARD_RETURN(is_seed(), false);
    }

    bool transfer_handle::is_finished() const
    {
        LIBED2K_FORWARD_RETURN(is_finished(), false);
    }

    bool transfer_handle::is_paused() const
    {
        LIBED2K_FORWARD_RETURN(is_paused(), false);
    }

    bool transfer_handle::is_aborted() const
    {
        LIBED2K_FORWARD_RETURN(is_aborted(), false);
    }

    bool transfer_handle::is_announced() const
    {
        LIBED2K_FORWARD_RETURN(is_announced(), false);
    }

    transfer_status transfer_handle::status() const
    {
        LIBED2K_FORWARD_RETURN(status(), transfer_status());
    }

    transfer_status::state_t transfer_handle::state() const
    {
        // TODO - some default status there?
        LIBED2K_FORWARD_RETURN(state(), transfer_status::queued_for_checking);
    }

    void transfer_handle::get_peer_info(std::vector<peer_info>& infos) const
    {
        LIBED2K_FORWARD(get_peer_info(infos));
    }

    void transfer_handle::piece_availability(std::vector<int>& avail) const
    {
        LIBED2K_FORWARD(piece_availability(avail));
    }

    void transfer_handle::set_piece_priority(int index, int priority) const
    {
        LIBED2K_FORWARD(set_piece_priority(index, priority));
    }

    int transfer_handle::piece_priority(int index) const
    {
        LIBED2K_FORWARD_RETURN(piece_priority(index), 0);
    }

	std::vector<int> transfer_handle::piece_priorities() const
    {
        std::vector<int> ret;
        LIBED2K_FORWARD_RETURN2(piece_priorities(&ret), std::vector<int>());
        return ret;
    }

    bool transfer_handle::is_sequential_download() const
    {
        LIBED2K_FORWARD_RETURN(is_sequential_download(), false);
    }

    void transfer_handle::set_sequential_download(bool sd) const
    {
        LIBED2K_FORWARD(set_sequential_download(sd));
    }

    void transfer_handle::set_upload_limit(int limit) const
    {
        LIBED2K_FORWARD(set_upload_limit(limit));
    }

    int transfer_handle::upload_limit() const
    {
        LIBED2K_FORWARD_RETURN(upload_limit(), 0);
    }

    void transfer_handle::set_download_limit(int limit) const
    {
        LIBED2K_FORWARD(set_download_limit(limit));
    }

    int transfer_handle::download_limit() const
    {
        LIBED2K_FORWARD_RETURN(download_limit(), 0);
    }

    void transfer_handle::set_upload_mode(bool b) const
    {
        LIBED2K_FORWARD(set_upload_mode(b));
    }

    void transfer_handle::pause() const
    {
        LIBED2K_FORWARD(pause());
    }

    void transfer_handle::resume() const
    {
        LIBED2K_FORWARD(resume());
    }

    size_t transfer_handle::num_pieces() const
    {
        LIBED2K_FORWARD_RETURN(num_pieces(), 0);
    }

    int transfer_handle::num_peers() const
    {
        LIBED2K_FORWARD_RETURN(num_peers(), 0);
    }

    int transfer_handle::num_seeds() const
    {
        LIBED2K_FORWARD_RETURN(num_seeds(), 0);
    }

    void transfer_handle::save_resume_data(int flags) const
    {
        LIBED2K_FORWARD(save_resume_data(flags));
    }

    void transfer_handle::move_storage(const std::string& save_path) const
    {
        LIBED2K_FORWARD(move_storage(save_path));
    }

    bool transfer_handle::rename_file(const std::string& name) const
    {
        LIBED2K_FORWARD_RETURN(rename_file(name), false);
    }

    std::string transfer_status2string(const transfer_status& s)
    {
        static std::string vstr[] =
        {
            std::string("queued_for_checking"),
            std::string("checking_files"),
            std::string("downloading_metadata"),
            std::string("downloading"),
            std::string("finished"),
            std::string("seeding"),
            std::string("allocating"),
            std::string("checking_resume_data")
        };

        if (s.state < sizeof(vstr)/sizeof(vstr[0]))
        {
            return vstr[s.state];
        }

        return std::string("unknown state");
    }

}
