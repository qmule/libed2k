
// ed2k peer connection

#ifndef __LIBED2K_PEER_CONNECTION__
#define __LIBED2K_PEER_CONNECTION__

#include <boost/smart_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/aligned_storage.hpp>

#include <libtorrent/intrusive_ptr_base.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/chained_buffer.hpp>
#include <libtorrent/buffer.hpp>
#include <libtorrent/disk_buffer_holder.hpp>
#include <libtorrent/time.hpp>
#include <libtorrent/io.hpp>

#include "types.hpp"
#include "error_code.hpp"

namespace libed2k
{
    class peer;
    class transfer;
    class base_socket;
    namespace aux{
        class session_impl;
    }

    namespace detail = libtorrent::detail;
    class peer_connection : public libtorrent::intrusive_ptr_base<peer_connection>,
                            public boost::noncopyable
    {
    public:

        enum channels
        {
            upload_channel,
            download_channel,
            num_channels
        };

        // this is the constructor where the we are the active part.
        // The peer_conenction should handshake and verify that the
        // other end has the correct id
        peer_connection(aux::session_impl& ses, boost::weak_ptr<transfer>,
                        boost::shared_ptr<base_socket> s,
                        const tcp::endpoint& remote, peer* peerinfo);

        // with this constructor we have been contacted and we still don't
        // know which transfer the connection belongs to
        peer_connection(aux::session_impl& ses, boost::shared_ptr<base_socket> s,
                        const tcp::endpoint& remote, peer* peerinfo);

        ~peer_connection();

        peer* peer_info() const { return m_peer_info; }

        const tcp::endpoint& remote() const { return m_remote; }

        // is called once every second by the main loop
        void second_tick();

        // DRAFT
        enum message_type
        {
            msg_interested,
            msg_not_interested,
            msg_have,
            msg_request,
            msg_piece,
            msg_cancel,

            num_supported_messages
        };

        // called from the main loop when this connection has any
        // work to do.
        void on_send_data(error_code const& error, std::size_t bytes_transferred);
        void on_receive_data(error_code const& error, std::size_t bytes_transferred);
        void on_receive_data_nolock(error_code const& error,
                                    std::size_t bytes_transferred);
        void on_sent(error_code const& error, std::size_t bytes_transferred);
        void on_receive(error_code const& error, std::size_t bytes_transferred);

        buffer::const_interval receive_buffer() const
        {
            if (m_recv_buffer.empty())
                return buffer::const_interval(0,0);
            return buffer::const_interval(&m_recv_buffer[0],
                                          &m_recv_buffer[0] + m_recv_pos);
        }

        bool allocate_disk_receive_buffer(int disk_buffer_size);
        char* release_disk_receive_buffer();
        void reset_recv_buffer(int packet_size);
        void cut_receive_buffer(int size, int packet_size);

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

        void incoming_piece(peer_request const& p, disk_buffer_holder& data);
        void incoming_piece_fragment(int bytes);
        void start_receive_piece(peer_request const& r);

        void send_block_requests();

        typedef void (peer_connection::*message_handler)(int received);

        // the following functions appends messages
        // to the send buffer
        // DRAFT
        void write_hello();
        void write_hello_answer();
        void write_request(const peer_request& r);
        void write_cancel(const peer_request& r);
        void write_have(int index);
        void write_piece(const peer_request& r, disk_buffer_holder& buffer);
        void write_handshake();

        enum message_type_flags { message_type_request = 1 };
        void send_buffer(char const* buf, int size, int flags = 0);
        void setup_send();

        enum sync_t { read_async, read_sync };
        void setup_receive(sync_t sync = read_sync);

        size_t try_read(sync_t s, error_code& ec);

        void on_timeout();
        // this will cause this peer_connection to be disconnected.
        void disconnect(error_code const& ec, int error = 0);
        bool is_disconnecting() const { return m_disconnecting; }

        // this is called when the connection attempt has succeeded
        // and the peer_connection is supposed to set m_connecting
        // to false, and stop monitor writability
        void on_connection_complete(error_code const& e);

        // called when it's time for this peer_conncetion to actually
        // initiate the tcp connection. This may be postponed until
        // the library isn't using up the limitation of half-open
        // tcp connections.
        void on_connect(int ticket);

        // this function is called after it has been constructed and properly
        // reference counted. It is safe to call self() in this function
        // and schedule events with references to itself (that is not safe to
        // do in the constructor).
        void start();

        // tells if this connection has data it want to send
        // and has enough upload bandwidth quota left to send it.
        bool can_write() const;
        bool can_read(char* state = 0) const;

        void set_upload_only(bool u);
        bool upload_only() const { return m_upload_only; }

    private:

        /**
          * initialize call back handlers
         */
        void init_handlers();

        void on_disk_write_complete(int ret, disk_io_job const& j,
                                    peer_request r, boost::shared_ptr<transfer> t);

        // protocol handlers
        void on_unhandled_packet(const error_code& error);
        void on_hello_packet(const error_code& error);
        void on_hello_answer(const error_code& error);

        // DRAFT
        enum state
        {
            read_packet_size,
            read_packet
        };

        // bw_idle: the channel is not used
        // bw_limit: the channel is waiting for quota
        // bw_network: the channel is waiting for an async write
        //   for read operation to complete
        // bw_disk: the peer is waiting for the disk io thread
        //   to catch up
        enum bw_state { bw_idle, bw_limit, bw_network, bw_disk };

        // state of on_receive
        state m_state;

        bool packet_finished() const { return m_packet_size <= m_recv_pos; }

        // upload and download channel state
        // enum from peer_info::bw_state
        char m_channel_state[2];

        static const message_handler m_message_handler[num_supported_messages];

        bool dispatch_message(int received);

        // a back reference to the session
        // the peer belongs to.
        aux::session_impl& m_ses;

        // keep the io_service running as long as we
        // have peer connections
        boost::asio::io_service::work m_work;

        // timeouts
        ptime m_last_receive;
        ptime m_last_sent;

        libtorrent::buffer m_recv_buffer;

        // if this peer is receiving a piece, this
        // points to a disk buffer that the data is
        // read into. This eliminates a memcopy from
        // the receive buffer into the disk buffer
        disk_buffer_holder m_disk_recv_buffer;

        libtorrent::chained_buffer m_send_buffer;

        boost::shared_ptr<base_socket> m_socket;

        // this is the peer we're actually talking to
        // it may not necessarily be the peer we're
        // connected to, in case we use a proxy
        tcp::endpoint m_remote;

        // this is the transfer this connection is
        // associated with. If the connection is an
        // incoming connection, this is set to zero
        // until the some info??? is received. Then it's
        // set to the transfer it belongs to.
        boost::weak_ptr<transfer> m_transfer;

        // the blocks we have reserved in the piece
        // picker and will request from this peer.
        std::vector<pending_block> m_request_queue;

        // the queue of blocks we have requested
        // from this peer
        std::vector<pending_block> m_download_queue;

        // this peer's peer info struct. This may
        // be 0, in case the connection is incoming
        // and hasn't been added to a transfer yet.
        peer* m_peer_info;

        // the ticket id from the connection queue.
        // This is used to identify the connection
        // so that it can be removed from the queue
        // once the connection completes
        int m_connection_ticket;

        // this is true until this socket has become
        // writable for the first time (i.e. the
        // connection completed). While connecting
        // the timeout will not be triggered. This is
        // because windows XP SP2 may delay connection
        // attempts, which means that the connection
        // may not even have been attempted when the
        // time out is reached.
        bool m_connecting;

		// set to true when this peer is only uploading
		bool m_upload_only;

        // this is true if this connection has been added
        // to the list of connections that will be closed.
        bool m_disconnecting;

        // a list of byte offsets inside the send buffer
        // the piece requests
        std::vector<int> m_requests_in_buffer;

        // the size (in bytes) of the bittorrent message
        // we're currently receiving
        int m_packet_size;

        // the number of bytes of the bittorrent payload
        // we've received so far
        int m_recv_pos;

        int m_disk_recv_buffer_size;

        template <std::size_t Size>
        class handler_storage
        {
        public:
            boost::aligned_storage<Size> bytes;
        };

        handler_storage<TORRENT_READ_HANDLER_MAX_SIZE> m_read_handler_storage;
        handler_storage<TORRENT_WRITE_HANDLER_MAX_SIZE> m_write_handler_storage;

        template <class Handler, std::size_t Size>
        class allocating_handler
        {
        public:
            allocating_handler(Handler const& h, handler_storage<Size>& s):
                handler(h), storage(s)
            {}

            template <class A0>
            void operator()(A0 const& a0) const
            {
                handler(a0);
            }

            template <class A0, class A1>
            void operator()(A0 const& a0, A1 const& a1) const
            {
                handler(a0, a1);
            }

            template <class A0, class A1, class A2>
            void operator()(A0 const& a0, A1 const& a1, A2 const& a2) const
            {
                handler(a0, a1, a2);
            }

            friend void* asio_handler_allocate(
                std::size_t size, allocating_handler<Handler, Size>* ctx)
            {
                return &ctx->storage.bytes;
            }

            friend void asio_handler_deallocate(
                void*, std::size_t, allocating_handler<Handler, Size>* ctx)
            {
            }

            Handler handler;
            handler_storage<Size>& storage;
        };

        template <class Handler>
        allocating_handler<Handler, TORRENT_READ_HANDLER_MAX_SIZE>
        make_read_handler(Handler const& handler)
        {
            return allocating_handler<Handler, TORRENT_READ_HANDLER_MAX_SIZE>(
                handler, m_read_handler_storage);
        }

        template <class Handler>
        allocating_handler<Handler, TORRENT_WRITE_HANDLER_MAX_SIZE>
        make_write_handler(Handler const& handler)
        {
            return allocating_handler<Handler, TORRENT_WRITE_HANDLER_MAX_SIZE>(
                handler, m_write_handler_storage);
        }
    };

}

#endif
