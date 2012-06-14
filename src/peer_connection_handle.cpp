#include <boost/shared_ptr.hpp>
#include "libed2k/peer_connection_handle.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/peer_connection.hpp"
#include "libed2k/util.hpp"

using libed2k::aux::session_impl;

namespace libed2k
{

#ifdef BOOST_NO_EXCEPTIONS
#define LIBED2K_PC_FORWARD(call) \
        if (!m_pses)\
        {\
            return;\
        }\
        boost::intrusive_ptr<peer_connection> pc = m_pses->find_peer_connection(m_np);\
        if (!pc) return;\
        session_impl::mutex_t::scoped_lock l(m_pses->m_mutex);\
        pc->call;

#define LIBED2K_PC_FORWARD_RETURN(call, def) \
        if (!m_pses)\
        {\
            return def;\
        }\
        boost::intrusive_ptr<peer_connection> pc = m_pses->find_peer_connection(m_np);\
        if (!pc) return def;\
        session_impl::mutex_t::scoped_lock l(m_pses->m_mutex);\
        return pc->call;
#else
#define LIBED2K_PC_FORWARD(call) \
        if (!m_pses) {throw_null_session_pointer();}\
        boost::intrusive_ptr<peer_connection> pc = m_pses->find_peer_connection(m_np);\
        if (!pc) throw_invalid_pc_handle();\
        session_impl::mutex_t::scoped_lock l(m_pses->m_mutex);\
        pc->call;


#define LIBED2K_PC_FORWARD_RETURN(call, def) \
        if (!m_pses) {throw_null_session_pointer();}\
        boost::intrusive_ptr<peer_connection> pc = m_pses->find_peer_connection(m_np);\
        if (!pc) throw_invalid_pc_handle();\
        session_impl::mutex_t::scoped_lock l(m_pses->m_mutex);\
        return pc->call;\


#endif

#ifndef BOOST_NO_EXCEPTIONS
    inline void throw_invalid_pc_handle()
    {
        throw libed2k_exception(errors::invalid_transfer_handle);
    }

    inline void throw_null_session_pointer()
    {
        throw libed2k_exception(errors::session_pointer_is_null);
    }
#endif

    peer_connection_handle::peer_connection_handle(): m_pses(NULL){}

    peer_connection_handle::peer_connection_handle(boost::intrusive_ptr<peer_connection> pc, aux::session_impl* pses) : m_pses(pses)
    {
        if (pc)
        {
            m_np = pc->get_network_point();
        }
    }

    void peer_connection_handle::send_message(const std::string& strMessage)
    {
        LIBED2K_PC_FORWARD(send_message(strMessage))
    }

    void peer_connection_handle::get_shared_files() const
    {
        LIBED2K_PC_FORWARD(request_shared_files())
    }

    void peer_connection_handle::get_shared_directories() const
    {
        LIBED2K_PC_FORWARD(request_shared_directories())
    }

    void peer_connection_handle::get_shared_directory_files(const std::string& strDirectory) const
    {
        LIBED2K_PC_FORWARD(request_shared_directory_files(strDirectory))
    }

    void peer_connection_handle::get_ismod_directory(const md4_hash& hash) const
    {
        LIBED2K_PC_FORWARD(request_ismod_directory_files(hash))
    }

    md4_hash peer_connection_handle::get_hash() const
    {
        LIBED2K_PC_FORWARD_RETURN(get_connection_hash(), md4_hash())
    }

    net_identifier peer_connection_handle::get_network_point() const
    {
        LIBED2K_PC_FORWARD_RETURN(get_network_point(), net_identifier())
    }

    peer_connection_options peer_connection_handle::get_options() const
    {
        LIBED2K_PC_FORWARD_RETURN(get_options(), peer_connection_options())
    }

    misc_options peer_connection_handle::get_misc_options() const
    {
        LIBED2K_PC_FORWARD_RETURN(get_misc_options(), misc_options())
    }

    misc_options2 peer_connection_handle::get_misc_options2() const
    {
        LIBED2K_PC_FORWARD_RETURN(get_misc_options2(), misc_options2())
    }

    bool peer_connection_handle::is_active() const
    {
        LIBED2K_PC_FORWARD_RETURN(is_active(), false)
    }

    bool peer_connection_handle::empty() const
    {
        if (!m_pses)
        {
            return (true);
        }

        return (m_np.empty());
    }

}
