#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "base_socket.hpp"

using boost::asio::ip::tcp;
using boost::asio::buffer;
namespace libed2k{


// handle incoming
class incoming_connection : public boost::enable_shared_from_this<incoming_connection>
{
public:
  typedef boost::shared_ptr<incoming_connection> pointer;

  static pointer create(boost::asio::io_service& io_service)
  {
      return pointer(new incoming_connection(io_service));
  }

  tcp::socket& socket()
  {
      return m_conn.socket();
  }


  void start()
  {
      m_conn.async_read_header(boost::bind(&incoming_connection::handle_read_header, shared_from_this(), boost::asio::placeholders::error));
  }

  ~incoming_connection()
  {
      std::cout << "connection was deleted\n";
  }
private:
  incoming_connection(boost::asio::io_service& io) : m_conn(io)
  {

  }

  void handle_read_header(const boost::system::error_code& error)
  {
      if (!error)
      {
          std::cout << "Read message header " << std::endl;
          //m_conn.context().dump();

          //if (m_conn.context().m_type == socketMessageType<HelloServer>::value)
          //{
          //    m_conn.async_read_body(m_s_hello, boost::bind(&client_connection::handle_read_client_hello, shared_from_this(), boost::asio::placeholders::error));
          //}

      }
  }

  void handle_read_client_hello(const boost::system::error_code& error)
  {
      if (!error)
      {
          std::cout << "Reseive client hello, answer\n";
          //m_conn.async_write(m_c_hello, boost::bind(&client_connection::handle_write_client_hello, shared_from_this(), boost::asio::placeholders::error));
      }
  }

  void handle_write_client_hello(const boost::system::error_code& error)
  {
      std::cout << "Write data to client " << std::endl;
      // wait on next message
      start();
  }

  base_socket m_conn;
};

// acceptor
class tcp_server
{
public:
    tcp_server(boost::asio::io_service& io) :
        m_acceptor(io, tcp::endpoint(tcp::v4(), 1303))
    {
        start_listening();
    }
private:

    void start_listening()
    {
        incoming_connection::pointer new_connection = incoming_connection::create(m_acceptor.get_io_service());

        m_acceptor.async_accept(new_connection->socket(), boost::bind(&tcp_server::handle_accept, this, new_connection,
                  boost::asio::placeholders::error));
    }

    void handle_accept(incoming_connection::pointer conn, const boost::system::error_code& error)
    {
        if (!error)
        {
            conn->start();
        }
        else
        {
            std::cout << "Error on accept: " << error.message() << std::endl;
        }

        start_listening();
    }

    tcp::acceptor m_acceptor;
};

//client
class client
{
public:
    client(boost::asio::io_service& service, boost::asio::ip::tcp::endpoint endpoint) : m_conn(service)
    {
        m_conn.socket().async_connect(endpoint, boost::bind(&client::handle_connect, this, boost::asio::placeholders::error));
    }

private:
    base_socket  m_conn;

    void handle_connect(const boost::system::error_code& error)
    {
        if (error)
        {
            do_close();
            return;
        }

        // after connect write message to server
        //m_conn.async_write(m_s_hello, boost::bind(&client::handle_write, this,
        //        boost::asio::placeholders::error,
        //        boost::asio::placeholders::bytes_transferred));
    }

    void handle_write(const boost::system::error_code& error, size_t nSize)
    {
        // after wrote data read answer
        if (!error)
        {
            m_conn.async_read_header(boost::bind(&client::handle_read_header, this, boost::asio::placeholders::error));
        }
    }

    void handle_read_header(const boost::system::error_code& error)
    {
        if (!error)
        {
            // read header complete - next read body
            // read answer
            /*
            if (m_conn.context().m_type == socketMessageType<HelloClient>::value)
            {
                m_conn.async_read_body(m_c_hello, boost::bind(&client::handle_read_server_hello, this, boost::asio::placeholders::error));
            }
            else
            {
                std::cout << "Unknown message type" << std::endl;
            }
            */
        }
    }

    void handle_read_server_hello(const boost::system::error_code& error)
    {
        if (!error)
        {
            // callback on read some server message
            /*
            std::cout << "Server answer hello\n";
            m_conn.async_write(m_s_hello, boost::bind(&client::handle_write, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
                */

        }
    }

    void do_close()
    {
        m_conn.socket().close();
    }

};


}

int main(int argc, char* argv[])
{
    boost::asio::io_service io;
    // our server endpoint
    boost::asio::ip::tcp::endpoint server_point(tcp::v4(), 4462);
    // ok, prepare end point
    tcp::resolver resolver(io);
    tcp::resolver::query query("el.is74.ru", "4461");
    tcp::resolver::iterator iterator = resolver.resolve(query); // use async resolve
    // eMule server end point
    boost::asio::ip::tcp::endpoint client_point = *iterator;



    return 0;
}

