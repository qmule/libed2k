
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
#include <libtorrent/disk_buffer_holder.hpp>
#include <libtorrent/time.hpp>
#include <libtorrent/io.hpp>
#include <libtorrent/bitfield.hpp>

#include "libed2k/base_connection.hpp"
#include "libed2k/types.hpp"
#include "libed2k/error_code.hpp"
#include "libed2k/packet_struct.hpp"


#define DECODE_PACKET(packet_struct, name)       \
    packet_struct name;                          \
    if (!decode_packet(name))                    \
    {                                            \
        close(errors::decode_packet_error);      \
    }

namespace libed2k
{
    class peer;
    class transfer;
    class md4_hash;
    class known_file;
    namespace aux{
        class session_impl;
    }

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

        void init();

        aux::session_impl& session() { return m_ses; }

        peer* get_peer() const { return m_peer; }
        void set_peer(peer* pi) { m_peer = pi; }

        const tcp::endpoint& remote() const { return m_remote; }
        const hash_set& remote_hashset() const { return m_remote_hashset; }

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

        bool allocate_disk_receive_buffer(int disk_buffer_size);
        char* release_disk_receive_buffer();

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
        virtual void close(const error_code& ec);

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
        bool has_ip_address(client_id_type nIP) const;
        void send_message(const std::string& strMessage);
        void request_shared_files();
        void request_shared_directories();
        void request_shared_directory_files(const std::string& strDirectory);

        misc_options get_misc_options() const { return m_misc_options; }
        misc_options2 get_misc_options2() const { return m_misc_options2; }
        std::string get_name() const { return m_strName; }
        int get_version() const { return m_nVersion; }
        int get_port() const { return m_nPort; }
        bool is_active() const { return m_active; }
    private:

        // constructor method
        void reset();
        bool attach_to_transfer(const md4_hash& hash);

        void request_block();
        bool add_request(piece_block const& b, int flags = 0);
        void send_block_requests();

        void fill_send_buffer();
        void send_data(const peer_request& r);
        void on_disk_read_complete(int ret, disk_io_job const& j, peer_request r, peer_request left);
        void receive_data(const peer_request& r);
        void on_disk_write_complete(int ret, disk_io_job const& j,
                                    peer_request r, peer_request left, boost::shared_ptr<transfer> t);
        void on_receive_data(const error_code& error, std::size_t bytes_transferred,
                             peer_request r, peer_request left);

        template<typename T>
        void do_write(T& t)
        {
            assert(m_channel_state[upload_channel] == bw_idle);
            base_connection::do_write(t);
        }

        // the following functions appends messages
        // to the send buffer
        void write_hello();
        void write_hello_answer();
        void write_file_request(const md4_hash& file_hash);
        void write_file_answer(const md4_hash& file_hash, const std::string& filename);
        void write_no_file(const md4_hash& file_hash);
        void write_filestatus_request(const md4_hash& file_hash);
        void write_file_status(const md4_hash& file_hash, const bitfield& status);
        void write_hashset_request(const md4_hash& file_hash);
        void write_hashset_answer(const md4_hash& file_hash, const std::vector<md4_hash>& hash_set);
        void write_start_upload(const md4_hash& file_hash);
        void write_queue_ranking(boost::uint16_t rank);
        void write_accept_upload();
        void write_cancel_transfer();
        void write_request_parts(client_request_parts_64 rp);
        void write_part(const peer_request& r);

        // protocol handlers
        void on_hello(const error_code& error);
        void on_hello_answer(const error_code& error);
        void on_file_request(const error_code& error);
        void on_file_answer(const error_code& error);
        void on_file_description(const error_code& error);
        void on_no_file(const error_code& error);
        void on_filestatus_request(const error_code& error);
        void on_file_status(const error_code& error);
        void on_hashset_request(const error_code& error);
        void on_hashset_answer(const error_code& error);
        void on_start_upload(const error_code& error);
        void on_queue_ranking(const error_code& error);
        void on_accept_upload(const error_code& error);
        void on_out_parts(const error_code& error);
        void on_cancel_transfer(const error_code& error);
        void on_end_download(const error_code& error);
        void on_shared_files_request(const error_code& error);
        void on_shared_files_answer(const error_code& error);
        void on_shared_files_denied(const error_code& error);
        void on_shared_directories_answer(const error_code& error);
        void on_shared_directory_files_answer(const error_code& error);
        void on_client_message(const error_code& error);
        void on_client_captcha_request(const error_code& error);
        void on_client_captcha_result(const error_code& error);

        template<typename Struct>
        void send_throw_meta_order(const Struct& s)
        {
            m_messages_order.push_back(client_meta_packet(s));
            if (!is_closed())
            {
                fill_send_buffer();
            }
        }

        template <typename Struct>
        void on_request_parts(const error_code& error)
        {
            if (!error)
            {
                DECODE_PACKET(Struct, rp);
                DBG("request parts " << rp.m_hFile << ": "
                    << "[" << rp.m_begin_offset[0] << ", " << rp.m_end_offset[0] << "]"
                    << "[" << rp.m_begin_offset[1] << ", " << rp.m_end_offset[1] << "]"
                    << "[" << rp.m_begin_offset[2] << ", " << rp.m_end_offset[2] << "]"
                    << " <== " << m_remote);
                for (size_t i = 0; i < 3; ++i)
                {
                    if (rp.m_begin_offset[i] < rp.m_end_offset[i])
                    {
                        m_requests.push_back(
                            mk_peer_request(rp.m_begin_offset[i], rp.m_end_offset[i]));
                    }
                }
                fill_send_buffer();
            }
            else
            {
                ERR("request parts error " << error.message() << " <== " << m_remote);
            }
        }

        template <typename Struct>
        void on_sending_part(const error_code& error)
        {
            if (!error)
            {
                DECODE_PACKET(Struct, sp);
                DBG("part " << sp.m_hFile
                    << " [" << sp.m_begin_offset << ", " << sp.m_end_offset << "]"
                    << " <== " << m_remote);

                peer_request r = mk_peer_request(sp.m_begin_offset, sp.m_end_offset);
                receive_data(r);
            }
            else
            {
                ERR("part error " << error.message() << " <== " << m_remote);
            }
        }

        // keep the io_service running as long as we
        // have peer connections
        boost::asio::io_service::work m_work;

        // timeouts
        ptime m_last_receive;
        ptime m_last_sent;

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
        hash_set m_remote_hashset;

        // the queue of requests we have got
        // from this peer
        std::vector<peer_request> m_requests;

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

        // this is true if this connection has been added
        // to the list of connections that will be closed.
        bool m_disconnecting;

        int m_disk_recv_buffer_size;

        /**
          * special order for client messages
          *
         */
        std::deque<client_meta_packet>  m_messages_order;

        enum channels
        {
            upload_channel,
            download_channel,
            num_channels
        };

        // bw_idle: the channel is not used
        // bw_limit: the channel is waiting for quota
        // bw_network: the channel is waiting for an async write
        //   for read operation to complete
        // bw_disk: the peer is waiting for the disk io thread
        //   to catch up
        enum bw_state { bw_idle, bw_limit, bw_network, bw_disk };

        // upload and download channel state
        char m_channel_state[2];

        // client information
        std::string     m_strName;
        int             m_nVersion;
        std::string     m_strModVersion;
        int             m_nModVersion;
        int             m_nPort;
        int             m_nUDPPort;
        int             m_nBuddyUDP;
        client_id_type  m_nBuddyIP;
        int             m_nBuddyPort;
        int             m_nClientVersion;
        int             m_nCompatibleClient;
        bool            m_bOsInfoSupport;
        bool            m_bValueBasedTypeTags;

        // initialized in hello answer
        misc_options    m_misc_options;
        misc_options2   m_misc_options2;
    };

}

#endif
