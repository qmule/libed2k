
// ed2k peer connection

#ifndef __LIBED2K_PEER_CONNECTION__
#define __LIBED2K_PEER_CONNECTION__

#include <boost/smart_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>

#include <libtorrent/intrusive_ptr_base.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/chained_buffer.hpp>

namespace libed2k
{

    typedef boost::asio::ip::tcp tcp;
    typedef libtorrent::error_code error_code;

    class peer_request;
    class disk_buffer_holder;
    namespace aux{
        class session_impl;
    }

    class peer_connection : public libtorrent::intrusive_ptr_base<peer_connection>,
                            public boost::noncopyable
    {
    public:
        peer_connection(aux::session_impl& ses, boost::shared_ptr<tcp::socket> s,
                        const tcp::endpoint& remote);
        ~peer_connection();

        // DRAFT
        enum message_type
        {
            msg_interested,
            msg_have,
            msg_request,
            msg_piece,
            msg_cancel,

            num_supported_messages
        };

        // called from the main loop when this connection has any
        // work to do.
        void on_sent(error_code const& error, std::size_t bytes_transferred);
        void on_receive(error_code const& error, std::size_t bytes_transferred);

        // the message handlers are called
        // each time a recv() returns some new
        // data, the last time it will be called
        // is when the entire packet has been
        // received, then it will no longer
        // be called. i.e. most handlers need
        // to check how much of the packet they
        // have received before any processing
        // DRAFT
        void on_keepalive();
        void on_interested(int received);
        void on_not_interested(int received);
        void on_have(int received);
        void on_request(int received);
        void on_piece(int received);
        void on_cancel(int received);

        typedef void (peer_connection::*message_handler)(int received);

        // the following functions appends messages
        // to the send buffer
        // DRAFT
        void write_interested();
        void write_not_interested();
        void write_request(peer_request const& r);
        void write_cancel(peer_request const& r);
        void write_have(int index);
        void write_piece(peer_request const& r, disk_buffer_holder& buffer);
        void write_handshake();

        enum message_type_flags { message_type_request = 1 };
        void send_buffer(char const* buf, int size, int flags = 0);
        void setup_send();

        // this will cause this peer_connection to be disconnected.
        void disconnect(error_code const& ec, int error = 0);
        bool is_disconnecting() const { return m_disconnecting; }

        // this function is called after it has been constructed and properly
        // reference counted. It is safe to call self() in this function
        // and schedule events with references to itself (that is not safe to
        // do in the constructor).
        void start();

    private:

        // DRAFT
        enum state
        {
            read_packet_size,
            read_packet
        };

        // state of on_receive
        state m_state;

        static const message_handler m_message_handler[num_supported_messages];

        bool dispatch_message(int received);

        // a back reference to the session
        // the peer belongs to.
        aux::session_impl& m_ses;

        libtorrent::chained_buffer m_send_buffer;

        boost::shared_ptr<tcp::socket> m_socket;

        // this is the peer we're actually talking to
        // it may not necessarily be the peer we're
        // connected to, in case we use a proxy
        tcp::endpoint m_remote;

        // this is true if this connection has been added
        // to the list of connections that will be closed.
        bool m_disconnecting;

        // a list of byte offsets inside the send buffer
        // the piece requests
        std::vector<int> m_requests_in_buffer;
    };

}

#endif
