#include "libed2k/transfer_handle.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/transfer.hpp"

using libed2k::aux::session_impl;

#ifdef BOOST_NO_EXCEPTIONS

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

#else

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


    md4_hash transfer_handle::hash() const
    {
        static const md4_hash empty_hash(md4_hash::m_emptyMD4Hash);
        LIBED2K_FORWARD_RETURN(hash(), empty_hash);
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

    void transfer_handle::pause() const
    {
        LIBED2K_FORWARD(pause());
    }

    void transfer_handle::resume() const
    {

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

    fs::path transfer_handle::save_path() const
    {
        LIBED2K_FORWARD_RETURN(filepath(), fs::path());
    }

}
