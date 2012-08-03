#ifndef __BASE_CONNECTION__
#define __BASE_CONNECTION__

#include <string>
#include <sstream>
#include <map>
#include <deque>
#include <boost/cstdint.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <libtorrent/assert.hpp>
#include <libtorrent/chained_buffer.hpp>
#include <libtorrent/intrusive_ptr_base.hpp>
#include <libtorrent/stat.hpp>

#include "libed2k/types.hpp"
#include "libed2k/log.hpp"
#include "libed2k/archive.hpp"
#include "libed2k/packet_struct.hpp"
#include "libed2k/constants.hpp"

namespace libed2k{

    enum{ header_size = sizeof( libed2k_header) };

    namespace aux{ class session_impl; }

    class base_connection: public libtorrent::intrusive_ptr_base<base_connection>,
                           public boost::noncopyable
    {
        friend class aux::session_impl;
    public:
        base_connection(aux::session_impl& ses);
        base_connection(aux::session_impl& ses, boost::shared_ptr<tcp::socket> s,
                        const tcp::endpoint& remote);
        virtual ~base_connection();

        virtual void close(const error_code& ec);

        /**
         * connection closed when his socket is not opened
         */
        bool is_closed() const { return !m_socket || !m_socket->is_open(); }
        const tcp::endpoint& remote() const { return m_remote; }
        boost::shared_ptr<tcp::socket> socket() { return m_socket; }

        const stat& statistics() const { return m_statistics; }

        typedef boost::iostreams::basic_array_source<char> Device;

    protected:

        struct message {
            libed2k_header header;
            std::string body;
        };

        // constructor method
        void reset();

        void do_read();
        void do_write();

        template <typename T>
        message make_message(const T& t)
        {
            message msg;

            msg.header.m_protocol = packet_type<T>::protocol;
            boost::iostreams::back_insert_device<std::string> inserter(msg.body);
            boost::iostreams::stream<boost::iostreams::back_insert_device<std::string> >
                s(inserter);

            // Serialize the data first so we know how large it is.
            archive::ed2k_oarchive oa(s);
            oa << const_cast<T&>(t);
            s.flush();
            // packet size without protocol type and packet body size field
            msg.header.m_size = body_size(t, msg.body) + 1;
            msg.header.m_type = packet_type<T>::value;

            return msg;
        }

        template<typename T>
        void do_write(const T& t) { do_write_message(make_message(t)); }

        void do_write_message(const message& msg);

        void copy_send_buffer(const char* buf, int size);

        template <class Destructor>
        void append_send_buffer(char* buffer, int size, Destructor const& destructor)
        { m_send_buffer.append_buffer(buffer, size, size, destructor); }

        int send_buffer_size() const { return m_send_buffer.size(); }
        int send_buffer_capacity() const { return m_send_buffer.capacity(); }

        virtual void on_timeout(const error_code& e);

        /**
         * call when socket got packets header
         */
        void on_read_header(const error_code& error, size_t nSize);

        /**
         * call when socket got packets body and call users call back
         */
        void on_read_packet(const error_code& error, size_t nSize);

        /**
         * order write handler - executed while message order not empty
         */
        void on_write(const error_code& error, size_t nSize);

        /**
         * deadline timer handler
         */
        void check_deadline();

        template <typename Struct>
        size_t body_size(const Struct& s, const std::string& body)
        { return body.size(); }

        template<typename size_type>
        size_t body_size(const client_sending_part<size_type>&s, const std::string& body)
        { return body.size() + s.m_end_offset - s.m_begin_offset; }

        /**
         * will call from external handlers for extract buffer into structure
         * on error return false
         */
        template<typename T>
        bool decode_packet(T& t)
        {
            try
            {
                boost::iostreams::stream_buffer<base_connection::Device> buffer(
                    &m_in_container[0], m_in_header.m_size - 1);
                std::istream in_array_stream(&buffer);
                archive::ed2k_iarchive ia(in_array_stream);
                ia >> t;
            }
            catch(libed2k_exception& e)
            {
                DBG("Error on conversion " << e.what());
                return (false);
            }

            return (true);
        }

        template <typename Self>
        boost::intrusive_ptr<Self> self_as()
        { return boost::intrusive_ptr<Self>((Self*)this); }

        template <typename Self>
        boost::intrusive_ptr<const Self> self_as() const
        { return boost::intrusive_ptr<const Self>((const Self*)this); }

        /**
         * handler on receive data packet
         * @param read data buffer
         * @param error code
         */
        typedef boost::function<void (const error_code&)> packet_handler;

        // handler storage type - first argument opcode + protocol
        typedef std::map<std::pair<proto_type, proto_type>, packet_handler> handler_map;

        void add_handler(std::pair<proto_type, proto_type> ptype, packet_handler handler);

        aux::session_impl& m_ses;
        boost::shared_ptr<tcp::socket> m_socket;
        dtimer m_deadline;     //!< deadline timer for reading operations
        libed2k_header m_in_header;    //!< incoming message header
        socket_buffer m_in_container; //!< buffer for incoming messages
        socket_buffer m_in_gzip_container; //!< buffer for compressed data
        chained_buffer m_send_buffer;  //!< buffer for outgoing messages
        tcp::endpoint m_remote;

        bool m_write_in_progress; //!< write indicator
        bool m_read_in_progress;  //!< read indicator

        handler_map m_handlers;

        // statistics about upload and download speeds
        // and total amount of uploads and downloads for
        // this connection
        stat m_statistics;

        //
        // Custom memory allocation for asynchronous operations
        //
        template <std::size_t Size>
        class handler_storage
        {
        public:
            boost::aligned_storage<Size> bytes;
        };

        handler_storage<READ_HANDLER_MAX_SIZE> m_read_handler_storage;
        handler_storage<WRITE_HANDLER_MAX_SIZE> m_write_handler_storage;

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
        allocating_handler<Handler, READ_HANDLER_MAX_SIZE>
        make_read_handler(Handler const& handler)
        {
            return allocating_handler<Handler, READ_HANDLER_MAX_SIZE>(
                handler, m_read_handler_storage);
        }

        template <class Handler>
        allocating_handler<Handler, WRITE_HANDLER_MAX_SIZE>
        make_write_handler(Handler const& handler)
        {
            return allocating_handler<Handler, WRITE_HANDLER_MAX_SIZE>(
                handler, m_write_handler_storage);
        }
    };
}

#endif
