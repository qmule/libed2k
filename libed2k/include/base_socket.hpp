#ifndef __BASE_SOCKET__
#define __BASE_SOCKET__

#include <string>
#include <sstream>
#include <map>
#include <boost/cstdint.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include "log.hpp"
#include "archive.hpp"
#include "packet_struct.hpp"

using boost::asio::ip::tcp;
using boost::asio::buffer;

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
	typedef boost::function<void (socket_buffer&, const boost::system::error_code&)> message_handler;

	/**
	  * callback container
	 */
	typedef std::map<proto_type, message_handler> callback_map;

	typedef void (*on_event)(const boost::system::error_code&);

	base_socket(boost::asio::io_service& io) :
	    m_socket(io),
	    m_unhandled_handler(NULL),
	    out_stream(std::ios_base::binary)
	{}

	boost::asio::ip::tcp::socket& socket()
	{
		return (m_socket);
	}

	 /**
	   * write structure into socket
	   *
	  */
	template <typename T, typename Handler>
	void async_write(T& t, Handler handler)
	{
		out_stream.str("");
		// Serialize the data first so we know how large it is.
		archive::ed2k_oarchive oa(out_stream);
		oa << t;

		LDBG_ << "stream size: " << out_stream.str().size();
		// generate header
		m_out_header.m_protocol = OP_EDONKEYPROT;
		m_out_header.m_size     = out_stream.str().size() + 1;  // packet size without protocol type and packet body size field
		m_out_header.m_type     = packet_type<T>::value;
		LDBG_ << "packet type: " <<  packetToString(packet_type<T>::value);

		// Write the serialized data to the socket. We use "gather-write" to send
		// both the header and the data in a single write operation.
		std::vector<boost::asio::const_buffer> buffers;
		buffers.push_back(boost::asio::buffer(&m_out_header, header_size));
		buffers.push_back(boost::asio::buffer(out_stream.str()));
		boost::asio::async_write(m_socket, buffers, handler);
	}

	/*
	template<typename Handler>
	void async_read_header(Handler handler)
	{
	    boost::asio::async_read(m_socket, boost::asio::buffer(&m_in_header, header_size), handler);
	}

	template<typename T, typename Handler>
	void async_read_body(T& t, Handler handler)
	{
		void (base_socket::*f)(const boost::system::error_code&, T&, boost::tuple<Handler>)
			    		= &base_socket::handle_read_body<T, Handler>;

		// increase internal buffer size if need
		if (m_in_container.size() < m_in_header.m_size - 1)
		{
			m_in_container.resize(m_in_header.m_size - 1);
		}

		boost::asio::async_read(m_socket, boost::asio::buffer(&m_in_container[0], m_in_header.m_size - 1),
				boost::bind(f, this, boost::asio::placeholders::error, boost::ref<T>(t), boost::make_tuple(handler)));
	}
*/
	/**
	  * read packet body and serialize it into type T
	 */
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

	/**
	  * start async read
	  * after reading completed - appropriate user callback will faired
	  * or callback for unhandled packets if it set
	 */
	void async_read()
	{
	    boost::asio::async_read(m_socket,
	            boost::asio::buffer(&m_in_header, header_size),
	            boost::bind(&base_socket::handle_read_header, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}

	/**
	  * read packet header
	 */
	const libed2k_header& context() const
	{
		return (m_in_header);
	}

	/**
	  * add ordinary callback
	 */
	void add_callback(proto_type ptype, message_handler handler)
	{
	    m_callbacks.insert(make_pair(ptype, handler));
	}

	/**
	  * add callback for unhandled operation
	 */
	void add_unhandled_callback(message_handler handler)
	{
	    m_unhandled_handler = handler;
	}
private:
	boost::asio::ip::tcp::socket	m_socket;       //!< operation socket
	message_handler                 m_unhandled_handler;
	std::ostringstream 				out_stream;     //!< output buffer
	libed2k_header					m_out_header;   //!< output header
	libed2k_header					m_in_header;    //!< incoming message header
	socket_buffer 				    m_in_container; //!< buffer for incoming messages
	callback_map                    m_callbacks;

	void handle_read_header(const boost::system::error_code& error, size_t nSize)
	{
	    if (!error)
	    {
	        // we must download body in any case
            // increase internal buffer size if need
            if (m_in_container.size() < m_in_header.m_size - 1)
            {
                m_in_container.resize(m_in_header.m_size - 1);
            }

            boost::asio::async_read(m_socket, boost::asio::buffer(&m_in_container[0], m_in_header.m_size - 1),
                    boost::bind(&base_socket::handle_read_packet, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	    }
	    else
	    {
	        // call error callback
	    }
	}

	void handle_read_packet(const boost::system::error_code& error, size_t nSize)
	{
	    if (!error)
	    {
	        //!< search appropriate dispatcher
	        callback_map::iterator itr = m_callbacks.find(m_in_header.m_type);

	        if (itr != m_callbacks.end())
	        {
                std::string strData = "ddfd";
	            LDBG_ << "call normal handler";
                LDBG_ << strData;
	            itr->second(m_in_container, error);
	        }
	        else
	        {
	            if (m_unhandled_handler)
	            {
	                LDBG_ << "call unhandled ";
	                m_unhandled_handler(m_in_container, error);
	            }
	            else
	            {
	                LDBG_ << "unhandled handler is null";
	            }
	        }

	    }
	    else
	    {
	        // call error callback
	    }
	}
};

}
#endif
