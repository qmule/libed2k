
#ifndef __LIBED2K_SERVER_CONNECTION__
#define __LIBED2K_SERVER_CONNECTION__

#include <boost/noncopyable.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/asio.hpp>

#include <libtorrent/intrusive_ptr_base.hpp>

#include "types.hpp"
#include "packet_struct.hpp"
#include "error_code.hpp"
#include "base_socket.hpp"

namespace libed2k
{
    namespace aux { class session_impl; }

    class server_connection: public libtorrent::intrusive_ptr_base<server_connection>,
                             public boost::noncopyable
    {
        friend class aux::session_impl;
    public:
        server_connection(const aux::session_impl& ses);

        void start();
        void close();

    private:

        void on_name_lookup(const error_code& error, tcp::resolver::iterator i);            //!< resolve host name go to connect
        void on_connection_complete(error_code const& e);                                   //!< connect to host name and go to start
        void on_unhandled_packet(base_socket::socket_buffer& sb, const error_code& error);

        void on_write_completed(const error_code& error, size_t nSize);                     //!< after write operation

        //!< server message handlers
        void on_reject(base_socket::socket_buffer& sb, const error_code& error);            //!< server reject last command
        void on_disconnect(base_socket::socket_buffer& sb, const error_code& error);        //!< disconnect signal received
        void on_server_message(base_socket::socket_buffer& sb, const error_code& error);    //!< server message received
        void on_server_list(base_socket::socket_buffer& sb, const error_code& error);       //!< server list received
        void on_users_list(base_socket::socket_buffer& sb, const error_code& error);        //!< users list from server
        void on_id_change(base_socket::socket_buffer& sb, const error_code& error);         //!< our id changed message

        boost::uint32_t                 m_nClientId;
        tcp::resolver                   m_name_lookup;
        boost::shared_ptr<base_socket>  m_socket;
        tcp::endpoint                   m_target;

        const aux::session_impl& m_ses;
    };
}

#endif
