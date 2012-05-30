#include "libed2k/peer_handle.hpp"
#include "libed2k/session_impl.hpp"
#include "libed2k/peer_connection.hpp"
#include "libed2k/util.hpp"

using libed2k::aux::session_impl;

namespace libed2k
{
    peer_handle::peer_handle() : m_peer(boost::weak_ptr<peer_connection>())
    {

    }

    void peer_handle::send_message(const std::string& strMessage)
    {
        LIBED2K_FORWARD(send_message(strMessage), peer_connection, m_peer);
    }

    void peer_handle::get_shared_files() const
    {
        LIBED2K_FORWARD(request_shared_files(), peer_connection, m_peer);
    }

    void peer_handle::get_shared_directories() const
    {
        LIBED2K_FORWARD(request_shared_directories(), peer_connection, m_peer);
    }

    void peer_handle::get_shared_directory_files(const std::string& strDirectory) const
    {
        LIBED2K_FORWARD(request_shared_directory_files(strDirectory), peer_connection, m_peer);
    }

    misc_options peer_handle::get_misc_options() const
    {
        LIBED2K_FORWARD_RETURN(get_misc_options(), misc_options(), peer_connection, m_peer);
    }

    misc_options2 peer_handle::get_misc_options2() const
    {
        LIBED2K_FORWARD_RETURN(get_misc_options2(), misc_options2(), peer_connection, m_peer);
    }

    std::string peer_handle::get_name() const
    {
        LIBED2K_FORWARD_RETURN(get_name(), std::string(""), peer_connection, m_peer);
    }

    int peer_handle::get_version() const
    {
        LIBED2K_FORWARD_RETURN(get_version(), -1, peer_connection, m_peer);
    }

    int peer_handle::get_port() const
    {
        LIBED2K_FORWARD_RETURN(get_port(), 0, peer_connection, m_peer);
    }

    bool peer_handle::is_active() const
    {
        LIBED2K_FORWARD_RETURN(is_active(), false, peer_connection, m_peer);
    }

}
