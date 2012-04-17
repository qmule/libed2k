#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "base_socket.hpp"

using boost::asio::ip::tcp;
using boost::asio::buffer;
namespace libed2k{


const char* toString(proto_type protocol)
{
    static const char* pchUnknown = "Unknown packet";
    static const char* chData[] =
    {
            "00",
            "OP_LOGINREQUEST",
            "02",
            "03",
            "04",
            "OP_REJECT",
            "06",
            "07",
            "08",
            "09",
            "0A",
            "0B",
            "0C",
            "0D",
            "0E",
            "0F",
            "10",
            "11",
            "12",
            "13",
            "OP_GETSERVERLIST", //14
            "OP_OFFERFILES",
            "OP_SEARCHREQUEST",
            "17",
            "OP_DISCONNECT",
            "OP_GETSOURCES",
            "OP_SEARCH_USER",
            "1B",
            "OP_CALLBACKREQUEST",
            "OP_QUERY_CHATS",
            "OP_CHAT_MESSAGE",
            "OP_JOIN_ROOM",
            "20",
            "OP_QUERY_MORE_RESULT",
            "22",
            "OP_GETSOURCES_OBFU",
            "24",
            "25",
            "26",
            "27",
            "28",
            "29",
            "2A",
            "2B",
            "2C",
            "2D",
            "2E",
            "2F",
            "30",
            "31",
            "OP_SERVERLIST",
            "OP_SEARCHRESULT",
            "OP_SERVERSTATUS",
            "OP_CALLBACKREQUESTED",
            "OP_CALLBACK_FAIL",
            "37",
            "OP_SERVERMESSAGE",
            "OP_CHAT_ROOM_REQUEST",
            "OP_CHAT_BROADCAST",
            "OP_CHAT_USER_JOIN",
            "OP_CHAT_USER_LEAVE",
            "OP_CHAT_USER",
            "3E","3F",
            "OP_IDCHANGE",
            "OP_SERVERIDENT",
            "OP_FOUNDSOURCES",
            "OP_USERS_LIST",
            "OP_FOUNDSOURCES_OBFU",
            "45",
            "OP_SENDINGPART",
            "OP_REQUESTPARTS",
            "OP_FILEREQANSNOFIL",
            "OP_END_OF_DOWNLOAD",
            "OP_ASKSHAREDFILES",
            "OP_ASKSHAREDFILESANSWER",
            "OP_HELLOANSWER",
            "OP_CHANGE_CLIENT_ID",//         = 0x4D, // <ID_old 4><ID_new 4> // Unused for sending
            "OP_MESSAGE",
            "OP_SETREQFILEID",
            "OP_FILESTATUS",
            "OP_HASHSETREQUEST",
            "OP_HASHSETANSWER",
            "53",
            "OP_STARTUPLOADREQ",
            "OP_ACCEPTUPLOADREQ",
            "OP_CANCELTRANSFER",
            "OP_OUTOFPARTREQS",
            "OP_REQUESTFILENAME",
            "OP_REQFILENAMEANSWER",
            "5A",
            "OP_CHANGE_SLOT",
            "OP_QUEUERANK",
            "OP_ASKSHAREDDIRS", //            = 0x5D, // (null)
            "OP_ASKSHAREDFILESDIR",
            "OP_ASKSHAREDDIRSANS",
            "OP_ASKSHAREDFILESDIRANS",
            "OP_ASKSHAREDDENIEDANS"//       = 0x61  // (null)
    };

    if (protocol < sizeof(chData)/sizeof(chData[0]))
    {
        return (chData[static_cast<unsigned int>(protocol)]);
    }

    return (pchUnknown);
}


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
          std::cout << "reseive: " << toString(m_conn.context().m_type) << std::endl;
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
    if (argc < 2)
    {
        return 0;
    }

    std::cout << libed2k::toString(atoi(argv[1])) << std::endl;
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

