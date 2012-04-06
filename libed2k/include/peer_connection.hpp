
// ed2k peer connection

#ifndef __LIBED2K_PEER_CONNECTION__
#define __LIBED2K_PEER_CONNECTION__

#include <boost/smart_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>

#include <libtorrent/intrusive_ptr_base.hpp>

namespace libed2k
{

    typedef boost::asio::ip::tcp tcp;

    namespace aux{
        class session_impl;
    }

    class peer_connection : public libtorrent::intrusive_ptr_base<peer_connection>,
                            public boost::noncopyable
    {
    public:
        peer_connection(aux::session_impl& ses, boost::shared_ptr<tcp::socket> s,
                        const tcp::endpoint& remote);

        bool is_disconnecting() const { return m_disconnecting; }

        // this function is called after it has been constructed and properly
        // reference counted. It is safe to call self() in this function
        // and schedule events with references to itself (that is not safe to
        // do in the constructor).
        void start();

    private:

        // a back reference to the session
        // the peer belongs to.
        aux::session_impl& m_ses;

        boost::shared_ptr<tcp::socket> m_socket;

        // this is the peer we're actually talking to
        // it may not necessarily be the peer we're
        // connected to, in case we use a proxy
        tcp::endpoint m_remote;

        // this is true if this connection has been added
        // to the list of connections that will be closed.
        bool m_disconnecting;

    };

}

#endif
