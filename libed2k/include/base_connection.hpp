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

#include "types.hpp"
#include "log.hpp"
#include "archive.hpp"
#include "packet_struct.hpp"
#include "constants.hpp"

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

        void close();

        /**
         * connection closed when his socket is not opened
         */
        bool is_closed() const { return !m_socket->is_open(); }
        const tcp::endpoint& remote() const { return m_remote; }

    protected:

        typedef boost::iostreams::basic_array_source<char> Device;

        // constructor method
        void init();

        void do_read();
        void do_write();

        template<typename T>
        void do_write(T& t);

        void copy_send_buffer(const char* buf, int size);

        template <class Destructor>
        void append_send_buffer(char* buffer, int size, Destructor const& destructor)
        { m_send_buffer.append_buffer(buffer, size, size, destructor); }

        virtual void on_error(const error_code& e);
        virtual void on_timeout(const error_code& e);

        typedef std::vector<char> socket_buffer;
        typedef libtorrent::chained_buffer chained_buffer;

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

        /**
         * will call from external handlers for extract buffer into structure
         * on error return false
         */
        template<typename T>
        bool decode_packet(T& t);

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
        // handler storage type
        typedef std::map<proto_type, packet_handler> handler_map;

        void add_handler(proto_type ptype, packet_handler handler);

        aux::session_impl& m_ses;
        boost::shared_ptr<tcp::socket> m_socket;
        dtimer m_deadline;     //!< deadline timer for reading operations
        libed2k_header m_in_header;    //!< incoming message header
        socket_buffer m_in_container; //!< buffer for incoming messages
        chained_buffer m_send_buffer;  //!< buffer for outgoing messages
        tcp::endpoint m_remote;
        bool m_write_in_progress; //!< write indicator
        handler_map m_handlers;

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

    template<typename T>
    void base_connection::do_write(T& t)
    {
        libed2k_header header;
        std::string body;
        boost::iostreams::back_insert_device<std::string> inserter(body);
        boost::iostreams::stream<boost::iostreams::back_insert_device<std::string> >
            s(inserter);

        // Serialize the data first so we know how large it is.
        archive::ed2k_oarchive oa(s);
        oa << t;
        s.flush();
        // packet size without protocol type and packet body size field
        header.m_size = body.size() + 1;
        header.m_type = packet_type<T>::value;

        copy_send_buffer((char*)(&header), header_size);
        copy_send_buffer(body.c_str(), body.size());

        do_write();
    }

    template<typename T>
    bool base_connection::decode_packet(T& t)
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
}

#endif
