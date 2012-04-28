#ifndef __BASE_SOCKET__
#define __BASE_SOCKET__

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

#include <libtorrent/assert.hpp>
#include <libtorrent/chained_buffer.hpp>

#include "types.hpp"
#include "log.hpp"
#include "archive.hpp"
#include "packet_struct.hpp"

namespace libed2k{

/**
  * base socket class for in-place handle protocol type(compression/decompression), write data with serialization and vice versa
  *
 */
class base_socket
{
public:
    enum{ header_size = sizeof( libed2k_header) };

    typedef std::vector<char> socket_buffer;
    typedef boost::iostreams::basic_array_source<char> Device;

    /**
      * call back on receive data packet
      * @param read data buffer
      * @param error code
     */
    typedef boost::function<void (const error_code&)> socket_handler;
    typedef std::map<proto_type, socket_handler> callback_map;                     //!< call backs storage type

    /**
      * @param io - operation io service
      * @param nTimeout - read operations timeout in seconds, 0 - means infinite
     */
    base_socket(boost::asio::io_service& io, int nTimeout);

    ~base_socket();

    /**
      * get internal socket
     */
    tcp::socket& socket();

    void set_timeout(int nTimeout);

    template<typename Handler>
    void do_connect(tcp::endpoint target, Handler handler)
    {
        m_deadline.expires_from_now(boost::posix_time::seconds(20));
        m_socket.async_connect(target, handler);
    }

    /**
      * do write your structure into socket
      * structure must be serialisable
     */
    template<typename T>
    void do_write(T& t)
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

        setup_send();
    }

    template <class Destructor>
    void append_send_buffer(char* buffer, int size, Destructor const& destructor)
	{
        m_send_buffer.append_buffer(buffer, size, size, destructor);
    }

    void setup_send();

    /**
     * write structure into socket
     * structure must have serialization method
     */
/*
 *  replace by do_write
    template <typename T, typename Handler>
    void async_write(T& t, Handler handler)
    {
        m_strOutput.clear();
        boost::iostreams::back_insert_device<std::string> inserter(m_strOutput);
        boost::iostreams::stream<boost::iostreams::back_insert_device<std::string> > s(inserter);

        // Serialize the data first so we know how large it is.
        archive::ed2k_oarchive oa(s);
        oa << t;
        s.flush();

        // generate header
        m_out_header.m_protocol = OP_EDONKEYPROT;
        m_out_header.m_size     = m_strOutput.size() + 1;  // packet size without protocol type and packet body size field
        m_out_header.m_type     = packet_type<T>::value;
        DBG("packet type: " <<  packetToString(packet_type<T>::value));

        // Write the serialized data to the socket. We use "gather-write" to send
        // both the header and the data in a single write operation.
        std::vector<boost::asio::const_buffer> buffers;
        buffers.push_back(boost::asio::buffer(&m_out_header, header_size));
        buffers.push_back(boost::asio::buffer(m_strOutput));
        boost::asio::async_write(m_socket, buffers, handler);
    }
    */

    /**
     * read packet body and serialize it into type T
     */
    // example code
/*
    template<typename T, typename Handler>
    void handle_read_body(const boost::system::error_code& error, T& t, boost::tuple<Handler> handler)
    {
        if (error)
        {
            boost::get<0>(handler)(error);
            return;
        }

        try
        {
            boost::iostreams::stream_buffer<Device> buffer(&m_in_container[0], m_in_header.m_size - 1);
            std::istream in_array_stream(&buffer);
            archive::ed2k_iarchive ia(in_array_stream);

            ia >> t;
            boost::get<0>(handler)(error);
        }
        catch(std::exception& e)
        {
            boost::get<0>(handler)(error);
        }
    }
*/
    /**
     * start async read
     * after reading completed - appropriate user callback will faired
     * or callback for unhandled packets if it set
     */
    void start_read_cycle();

    /**
      * read packet header
     */
    const libed2k_header& context() const;

    /**
      * add ordinary callbacks, set unhandled callback and error callback
     */
    void add_callback(proto_type ptype, socket_handler handler);
    void set_unhandled_callback(socket_handler handler);
    void set_error_callback(socket_handler handler);    //!< TODO - do we need it?
    void set_timeout_callback(socket_handler handler);


    /**
      * this method will call from external handlers for extract buffer into structure
      * on error return false
     */
    template<typename T>
    bool decode_packet(T& t)
    {
        try
        {
            boost::iostreams::stream_buffer<base_socket::Device> buffer(&m_in_container[0], m_in_header.m_size - 1);
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

    /**
     * stop all operations and close socket
     */
    void close();
private:
    tcp::socket    m_socket;           //!< operation socket
    socket_handler m_unhandled_handler;
    socket_handler m_handle_error;     //!< on error
    socket_handler m_timeout_handler;  //!< on timeout
    dtimer         m_deadline;         //!< deadline timer for reading operations
    int            m_timeout;          //!< deadline timeout for reading operations
    bool           m_stopped;          //!< socket is stopped
    bool           m_write_in_progress;//!< write indicator
    libed2k_header m_in_header;        //!< incoming message header
    socket_buffer  m_in_container;     //!< buffer for incoming messages
    callback_map   m_callbacks;

    //std::deque<std::pair<libed2k_header, std::string> > m_write_order;
    //!< outgoing messages order
    libtorrent::chained_buffer m_send_buffer;

    // TODO: should use memory pool instead of raw heap
    void copy_send_buffer(const char* buf, int size);

    std::pair<char*, int> allocate_buffer(int size);
    void free_buffer(char* buf, int size);

    /**
     * order write handler - executed while message order not empty
     */
    void handle_write(const error_code& error, size_t nSize);

    /**
     * common error handle and call user on error callback
     */
    void handle_error(const error_code& error);

    /**
     * call when socket got packets header
     */
    void handle_read_header(const error_code& error, size_t nSize);

    /**
     * call when socket got packets body and call users call back
     */
    void handle_read_packet(const error_code& error, size_t nSize);

    /**
     * deadline timer handler
     */
    void check_deadline();
};

}
#endif
