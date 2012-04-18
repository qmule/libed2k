
#ifndef __LIBED2K_SERVER_CONNECTION__
#define __LIBED2K_SERVER_CONNECTION__

#include <boost/noncopyable.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/asio.hpp>

#include <libtorrent/intrusive_ptr_base.hpp>

#include "types.hpp"
#include "error_code.hpp"
#include "base_socket.hpp"

namespace libed2k
{
    namespace aux { class session_impl; }

    class server_connection: public libtorrent::intrusive_ptr_base<server_connection>,
                             public boost::noncopyable
    {
    public:
        server_connection(const aux::session_impl& ses);

        void start();
        void close();

    private:

        void on_name_lookup(const error_code& error, tcp::resolver::iterator i);
        void on_connection_complete(error_code const& e);

        tcp::resolver m_name_lookup;
        boost::shared_ptr<base_socket> m_socket;
        tcp::endpoint m_target;

        const aux::session_impl& m_ses;
    };
}

#endif
