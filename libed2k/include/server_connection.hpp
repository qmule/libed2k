
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
#include "peer.hpp"

namespace libed2k
{
    namespace aux { class session_impl; }

    class server_request {
    public:
        server_request()
        {}

        std::string file;
    };

    class server_response {
    public:
        server_response()
        {}

        std::vector<peer_entry> peers;
    };


    class server_connection: public libtorrent::intrusive_ptr_base<server_connection>,
                             public boost::noncopyable
    {
        friend class aux::session_impl;
    public:
        server_connection(aux::session_impl& ses);

        void start();
        void close();

        /**
          * connection stopped when his socket is not opened
         */
        bool is_stopped() const;

        /**
          * after connect to server we read some messages from it
          * when we get new client id - it means connection accepted and initialized
         */
        bool is_initialized() const;

        const tcp::endpoint& getServerEndpoint() const;

        void write_announce(const std::string& filename, const md4_hash& filehash,
                            size_t filesize);

        void post_search_request(search_request& sr);
        void post_sources_request(const md4_hash& hFile, boost::uint64_t nSize);
    private:

        void on_name_lookup(const error_code& error, tcp::resolver::iterator i);            //!< resolve host name go to connect
        void on_connection_complete(error_code const& e);                                   //!< connect to host name and go to start
        void on_unhandled_packet(const error_code& error);
        void on_error(const error_code& error);

        //!< server message handlers
        void on_reject(const error_code& error);            //!< server reject last command
        void on_disconnect(const error_code& error);        //!< disconnect signal received
        void on_server_message(const error_code& error);    //!< server message received
        void on_server_list(const error_code& error);       //!< server list received
        void on_server_status(const error_code& error);     //!< server status
        void on_users_list(const error_code& error);        //!< users list from server
        void on_id_change(const error_code& error);         //!< our id changed message
        void on_server_ident(const error_code& error);      //!< server identification message
        void on_found_sources(const error_code& error);     //!< found sources message
        void on_search_result(const error_code& error);     //!< search result message
        void on_callback_request(const error_code& error);  //!< callback requested

        void handle_error(const error_code& error);

        void write_server_keep_alive();

        boost::uint32_t                 m_nClientId;
        tcp::resolver                   m_name_lookup;
        dtimer                          m_keep_alive;       //!< timer for ping server
        server_status                   m_server_status;    //!< server status info
        aux::session_impl&              m_ses;
        boost::uint32_t                 m_nFilesCount;
        boost::uint32_t                 m_nUsersCount;

        boost::shared_ptr<base_socket>  m_socket;
        tcp::endpoint                   m_target;
    };
}

#endif
