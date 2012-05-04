
// ed2k peer connection

#ifndef __LIBED2K_PEER_CONNECTION__
#define __LIBED2K_PEER_CONNECTION__

#include <boost/smart_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/aligned_storage.hpp>

#include <libtorrent/error_code.hpp>
#include <libtorrent/chained_buffer.hpp>
#include <libtorrent/buffer.hpp>
#include <libtorrent/disk_buffer_holder.hpp>
#include <libtorrent/time.hpp>
#include <libtorrent/io.hpp>
#include <libtorrent/bitfield.hpp>

#include "base_connection.hpp"
#include "types.hpp"
#include "error_code.hpp"

namespace libed2k
{
    class peer;
    class transfer;
    class md4_hash;
    class known_file;
    namespace aux{
        class session_impl;
    }

    namespace detail = libtorrent::detail;
    class peer_connection : public base_connection
    {
    public:

        // this is the constructor where the we are the active part.
        // The peer_conenction should handshake and verify that the
        // other end has the correct id
        peer_connection(aux::session_impl& ses, boost::weak_ptr<transfer>,
                        boost::shared_ptr<tcp::socket> s,
                        const tcp::endpoint& remote, peer* peerinfo);

        // with this constructor we have been contacted and we still don't
        // know which transfer the connection belongs to
        peer_connection(aux::session_impl& ses, boost::shared_ptr<tcp::socket> s,
                        const tcp::endpoint& remote, peer* peerinfo);

        ~peer_connection();

        peer* get_peer() const { return m_peer; }
        void set_peer(peer* pi) { m_peer = pi; }

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

        void incoming_piece(peer_request const& p, disk_buffer_holder& data);
        void incoming_piece_fragment(int bytes);
        void start_receive_piece(peer_request const& r);

        void on_timeout();
        // this will cause this peer_connection to be disconnected.
        void disconnect(error_code const& ec, int error = 0);
        bool is_disconnecting() const { return m_disconnecting; }

        // returns true if this connection is still waiting to
        // finish the connection attempt
        bool is_connecting() const { return m_connecting; }

        // this is called when the connection attempt has succeeded
        // and the peer_connection is supposed to set m_connecting
        // to false, and stop monitor writability
        void on_connect(const error_code& e);
        void on_error(const error_code& e);

        // called when it's time for this peer_conncetion to actually
        // initiate the tcp connection. This may be postponed until
        // the library isn't using up the limitation of half-open
        // tcp connections.
        void connect(int ticket);

        // a connection is local if it was initiated by us.
        // if it was an incoming connection, it is remote
        bool is_local() const { return m_active; }

        // this function is called after it has been constructed and properly
        // reference counted. It is safe to call self() in this function
        // and schedule events with references to itself (that is not safe to
        // do in the constructor).
        void start();

        int picker_options() const;

        // tells if this connection has data it want to send
        // and has enough upload bandwidth quota left to send it.
        bool can_write() const;
        bool can_read(char* state = 0) const;

        void set_upload_only(bool u);
        bool upload_only() const { return m_upload_only; }

    private:

        // constructor method
        void reset();
        bool attach_to_transfer(const md4_hash& hash);

        void request_block();
        bool add_request(piece_block const& b, int flags = 0);
        void send_block_requests();

        void on_disk_write_complete(int ret, disk_io_job const& j,
                                    peer_request r, boost::shared_ptr<transfer> t);

        // the following functions appends messages
        // to the send buffer
        void write_hello();
        void write_hello_answer();
        void write_file_request(const md4_hash& file_hash);
        void write_no_file(const md4_hash& file_hash);
        void write_file_status(const md4_hash& file_hash, const bitfield& status);
        void write_hashset_request(const md4_hash& file_hash);
        void write_hashset_answer(const known_file& file);
        void write_start_upload(const md4_hash& file_hash);
        void write_queue_ranking(boost::uint16_t rank);
        void write_accept_upload();
        void write_cancel_transfer();
        void write_have(int index);
        void write_piece(const peer_request& r, disk_buffer_holder& buffer);

        // protocol handlers
        void on_hello(const error_code& error);
        void on_hello_answer(const error_code& error);
        void on_file_request(const error_code& error);
        void on_piece(int received);

        bool packet_finished() const { return m_packet_size <= m_recv_pos; }

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

        // this is the transfer this connection is
        // associated with. If the connection is an
        // incoming connection, this is set to zero
        // until the some info??? is received. Then it's
        // set to the transfer it belongs to.
        boost::weak_ptr<transfer> m_transfer;

        // the pieces the other end have
        bitfield m_available_pieces;

        // the blocks we have reserved in the piece
        // picker and will request from this peer.
        std::vector<pending_block> m_request_queue;

        // the queue of blocks we have requested
        // from this peer
        std::vector<pending_block> m_download_queue;

        // the number of request we should queue up
        // at the remote end.
        boost::uint8_t m_desired_queue_size;

        // this peer's peer info struct. This may
        // be 0, in case the connection is incoming
        // and hasn't been added to a transfer yet.
        peer* m_peer;

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

        // is true if it was we that connected to the peer
        // and false if we got an incoming connection
        // could be considered: true = local, false = remote
        bool m_active;

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

    };

}

#endif
