#ifndef __LIBED2K_PEER_CONNECTION__
#define __LIBED2K_PEER_CONNECTION__

#include <boost/smart_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>

#include "libed2k/io.hpp"
#include "libed2k/piece_block_progress.hpp"
#include "libed2k/bitfield.hpp"
#include "libed2k/disk_buffer_holder.hpp"
#include "libed2k/base_connection.hpp"
#include "libed2k/error_code.hpp"
#include "libed2k/packet_struct.hpp"
#include "libed2k/peer_request.hpp"
#include "libed2k/piece_picker.hpp"
#include "libed2k/peer_info.hpp"
#include "libed2k/piece_block_progress.hpp"

#define DECODE_PACKET(packet_struct, name)       \
    packet_struct name;                          \
    if (!decode_packet(name))                    \
    {                                            \
        disconnect(errors::decode_packet_error); \
    }

namespace libed2k
{
    enum EClientSoftware
    {
        SO_EMULE            = 0,
        SO_CDONKEY          = 1,
        SO_LXMULE           = 2,
        SO_AMULE            = 3,
        SO_SHAREAZA         = 4,
        SO_EMULEPLUS        = 5,
        SO_HYDRANODE        = 6,
        SO_NEW2_MLDONKEY    = 0x0a,
        SO_LPHANT           = 0x14,
        SO_NEW2_SHAREAZA    = 0x28,
        SO_EDONKEYHYBRID    = 0x32,
        SO_EDONKEY          = 0x33,
        SO_MLDONKEY         = 0x34,
        SO_OLDEMULE         = 0x35,
        SO_UNKNOWN          = 0x36,
        SO_NEW_SHAREAZA     = 0x44,
        SO_NEW_MLDONKEY     = 0x98,
        SO_LIBED2K          = 0x99,
        SO_QMULE            = 0xA0,
        SO_COMPAT_UNK       = 0xFF
    };

    EClientSoftware uagent2csoft(const md4_hash& ua_hash);


    class peer;
    class transfer;
    class md4_hash;
    class known_file;
    namespace aux{
        class session_impl;
    }
    struct disk_io_job;

    struct pending_block
    {
        pending_block(const piece_block& b, size_type fsize):
            skipped(0), not_wanted(false), timed_out(false), busy(false), block(b),
            data_left(block_range(b.piece_index, b.block_index, fsize)), buffer(NULL),
            create_time(time_now()){}

        // the number of times the request
        // has been skipped by out of order blocks
        boost::uint16_t skipped;

        // if any of these are set to true, this block
        // is not allocated
        // in the piece picker anymore, and open for
        // other peers to pick. This may be caused by
        // it either timing out or being received
        // unexpectedly from the peer
        bool not_wanted:1;
        bool timed_out:1;

        // the busy flag is set if the block was
        // requested from another peer when this
        // request was queued. We only allow a single
        // busy request at a time in each peer's queue
        bool busy:1;

        piece_block block;
        // block covering
        range<size_type> data_left;
        // disk receive buffer
        char* buffer;
        // time when this block has been created
        ptime create_time;

        bool operator==(const pending_block& b)
        {
            return b.skipped == skipped && b.block == block
                && b.not_wanted == not_wanted && b.timed_out == timed_out;
        }

        void complete(const std::pair<size_type,size_type>& range) { data_left -= range; }

        bool completed() const { return data_left.empty(); }
    };

    struct has_block
    {
        has_block(const piece_block& b): block(b) {}
        const piece_block& block;
        bool operator()(const pending_block& pb) const
        { return pb.block == block; }
    };

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
        boost::weak_ptr<transfer> get_transfer() { return m_transfer; }
        bool has_transfer() const { return !m_transfer.expired(); }

        peer* get_peer() const { return m_peer; }
        void set_peer(peer* pi) { m_peer = pi; }
        unsigned short user_port() const;
        const tcp::endpoint& remote() const { return m_remote; }
        const bitfield& remote_pieces() const { return m_remote_pieces; }

        void get_peer_info(peer_info& p) const;

        enum peer_speed_t { slow = 1, medium, fast };
        peer_speed_t peer_speed();
        double peer_rate();

        // is called once every second by the main loop
        void second_tick(int tick_interval_ms);

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

        virtual void on_timeout();
        virtual void on_sent(const error_code& e, std::size_t bytes_transferred);

        virtual void disconnect(const error_code& ec, int error = 0);

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
        bool can_request() const;
        bool is_seed() const;

        const std::vector<pending_block>& download_queue() const;
        const std::vector<pending_block>& request_queue() const;

        void send_message(const std::string& strMessage);
        void request_shared_files();
        void request_shared_directories();
        void request_shared_directory_files(const std::string& strDirectory);
        void request_ismod_directory_files(const md4_hash& hash);

        misc_options get_misc_options() const { return m_misc_options; }
        misc_options2 get_misc_options2() const { return m_misc_options2; }

        bool is_active() const { return m_active; }
        net_identifier get_network_point() const;
        md4_hash get_connection_hash() const { return m_hClient; }
        peer_connection_options get_options() const { return m_options; }

        bool has_network_point(const net_identifier& np) const;
        bool has_hash(const md4_hash& hash) const;
        bool operator==(const net_identifier& np) const;
        bool operator==(const md4_hash& hash) const;

        boost::optional<piece_block_progress> downloading_piece_progress() const {
            return boost::optional<piece_block_progress>();
        }

        void send_block_requests();
        void cancel_all_requests();

        void assign_bandwidth(int channel, int amount);
        int bandwidth_throttle(int channel) const
        { return m_bandwidth_channel[channel].throttle(); }

        void set_upload_limit(int limit);
        void set_download_limit(int limit);
        int upload_limit() const { return m_upload_limit; }
        int download_limit() const { return m_download_limit; }

    private:

        // constructor method
        void reset();
        bool attach_to_transfer(const md4_hash& hash);

        virtual void do_read();
        virtual void do_write(int quota = std::numeric_limits<int>::max());

        int request_upload_bandwidth(
            bandwidth_channel* bwc1, bandwidth_channel* bwc2 = 0,
            bandwidth_channel* bwc3 = 0, bandwidth_channel* bwc4 = 0);
        int request_download_bandwidth(
            bandwidth_channel* bwc1, bandwidth_channel* bwc2 = 0,
            bandwidth_channel* bwc3 = 0, bandwidth_channel* bwc4 = 0);
        bool has_upload_bandwidth();
        bool has_download_bandwidth();

        void request_block();
        // adds a block to the request queue
        // returns true if successful, false otherwise
        enum flags_t { req_time_critical = 1, req_busy = 2 };
        bool add_request(const piece_block& b, int flags = 0);
        void abort_all_requests();
        void abort_expired_requests();
        bool requesting(const piece_block& b) const;
        size_t num_requesting_busy_blocks() const;
        int outstanding_bytes() const;

        void send_deferred();
        void fill_send_buffer();
        void send_data(const peer_request& r);
        void on_disk_read_complete(int ret, disk_io_job const& j, peer_request r, peer_request left);
        void receive_data(const peer_request& r);
        void receive_data();
        void on_disk_write_complete(int ret, disk_io_job const& j,
                                    peer_request req, boost::shared_ptr<transfer> t);
        void on_receive_data(const error_code& error, std::size_t bytes_transferred);
        void skip_data(char* buf = NULL);
        void on_skip_data(const error_code& error, std::size_t bytes_transferred, char* buf);

        template<typename T>
        void write_struct(T& t)
        {
            if ((m_channel_state[upload_channel] & peer_info::bw_seq) == 0)
                base_connection::write_struct(t);
            else
                defer_write(t);
        }

        // the following functions appends messages
        // to the send buffer
        void write_hello();
        void write_hello_answer();
        void write_ext_hello();
        void write_ext_hello_answer();
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
        void on_ext_hello(const error_code& error);
        void on_ext_hello_answer(const error_code& error);
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
        void on_ismod_files_request(const error_code& error);
        void on_shared_files_answer(const error_code& error);
        void on_shared_files_denied(const error_code& error);
        void on_shared_directories_request(const error_code& error);
        void on_shared_directories_answer(const error_code& error);
        void on_shared_directory_files_request(const error_code& error);
        void on_shared_directory_files_answer(const error_code& error);
        void on_ismod_directory_files(const error_code& error);
        void on_client_message(const error_code& error);
        void on_client_captcha_request(const error_code& error);
        void on_client_captcha_result(const error_code& error);
        template <typename Struct> void on_request_parts(const error_code& error);
        template <typename Struct> void on_sending_part(const error_code& error);

        template<typename T> void defer_write(const T& t);
        template<typename T> void send_throw_meta_order(const T& t);

        // keep the io_service running as long as we
        // have peer connections
        boost::asio::io_service::work m_work;

        // timeouts
        ptime m_last_receive;
        ptime m_last_sent;
        time_duration m_timeout;

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
        bitfield m_remote_pieces;

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
        size_t m_desired_queue_size;

        // the maximum number of busy blocks we can
        // request at a time
        size_t m_max_busy_blocks;

        // the bandwidth channels, upload and download
        // keeps track of the current quotas
        bandwidth_channel m_bandwidth_channel[num_channels];

        // number of bytes this peer can send and receive
        int m_quota[2];

        // this is the priority with which this peer gets
        // download bandwidth quota assigned to it.
        int m_priority;

        int m_upload_limit;
        int m_download_limit;

        // this peer's peer info struct. This may
        // be 0, in case the connection is incoming
        // and hasn't been added to a transfer yet.
        peer* m_peer;

        // this is a measurement of how fast the peer
        // it allows some variance without changing
        // back and forth between states
        peer_speed_t m_speed;

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

        int m_disk_recv_buffer_size;

        // this flag will active after hello -> hello_answer order
        bool m_handshake_complete;

        std::deque<message> m_deferred;

        // client information
        md4_hash        m_hClient;
        peer_connection_options m_options;

        // initialized in hello answer
        misc_options    m_misc_options;
        misc_options2   m_misc_options2;

        // the number of bytes of payload we've received so far
        int m_recv_pos;
        // current recuest processed
        peer_request m_recv_req;
    };
}

#endif
