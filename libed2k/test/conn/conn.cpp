#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "base_socket.hpp"
#include "log.hpp"

using boost::asio::ip::tcp;
using boost::asio::buffer;
namespace libed2k{

unsigned long nAddr;

// handle incoming
class incoming_connection : public boost::enable_shared_from_this<incoming_connection>
{
public:
  typedef boost::shared_ptr<incoming_connection> pointer;

  static pointer create(boost::asio::io_service& io_service)
  {
      pointer p = pointer(new incoming_connection(io_service));
      p->m_conn.add_unhandled_callback(boost::bind(&incoming_connection::handle_unhandled_packets, p, _1, _2));
      p->m_conn.add_callback(OP_LOGINREQUEST, boost::bind(&incoming_connection::handle_read_hello, p, _1, _2));
      return (p);
  }

  tcp::socket& socket()
  {
      return m_conn.socket();
  }


  void start()
  {
      m_conn.async_read();
  }

  ~incoming_connection()
  {
      std::cout << "connection was deleted\n";
  }
private:
  incoming_connection(boost::asio::io_service& io) : m_conn(io)
  {

  }

  void handle_write_client_hello(base_socket::socket_buffer& sb, const error_code& error)
  {
      if (!error)
      {
          LDBG_ << "server wrote packet ";
          m_conn.async_read();
      }
      else
      {
          LERR_ << "error " << error.message();
      }
  }

  void handle_read_hello(base_socket::socket_buffer& sb, const error_code& error)
  {
      if (!error)
      {
          LDBG_ << "server read packet " << packetToString(m_conn.context().m_type);
          m_conn.async_read();
      }
      else
      {
          LERR_ << "error " << error.message();
      }
  }

  void handle_unhandled_packets(base_socket::socket_buffer& sb, const error_code& error)
  {
      if (!error)
      {
          LDBG_ << "server receive unhandled packet";
          m_conn.async_read();
      }
      else
      {
          LERR_ << "error " << error.message();
      }
  }

  base_socket m_conn;
};

// acceptor
class tcp_server
{
public:
    tcp_server(boost::asio::io_service& io, tcp::endpoint endp) :
        m_acceptor(io, tcp::endpoint(endp))
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
    client(boost::asio::io_service& service, boost::asio::ip::tcp::endpoint endpoint) :
        m_conn(service)
    {
        m_conn.add_callback(OP_SERVERMESSAGE, boost::bind(&client::on_read_server_message, this, _1, _2)); // add callback
        m_conn.add_unhandled_callback(boost::bind(&client::on_read_unhandled_message, this, _1, _2));
        m_conn.socket().async_connect(endpoint, boost::bind(&client::handle_connect, this, boost::asio::placeholders::error));
    }

private:
    base_socket  m_conn;

    void handle_connect(const boost::system::error_code& error)
    {
        LDBG_ << "connect ";


        //m_conn.async_read_header(boost::bind(&client::handle_read_header, this, boost::asio::placeholders::error));

        if (error)
        {
            LDBG_ << " error";
            do_close(error);
            return;
        }

        boost::uint32_t nVersion = 0x3c;
        boost::uint32_t nCapability = CAPABLE_AUXPORT | CAPABLE_NEWTAGS | CAPABLE_UNICODE | CAPABLE_LARGEFILES;
        boost::uint32_t nClientVersion  = (3 << 24) | (2 << 17) | (3 << 10) | (1 << 7);

        // prepare hello packet
        m_hLocal = md4_hash::m_emptyMD4Hash;
        m_hLocal[0] = 120;
        m_hLocal[1] = 20;
        m_hLocal[2] = 10;

        m_hLocal[5] = 14;
        m_hLocal[14] = 111;
        m_login.m_hClient = m_hLocal;
        m_login.m_nClientId = 0; //nAddr;
        m_login.m_nPort     = 4662;

        m_login.m_list.add_tag(make_string_tag(std::string("http://www.aMule.org"), CT_NAME,true));
        m_login.m_list.add_tag(make_typed_tag(nVersion, CT_VERSION, true));
        m_login.m_list.add_tag(make_typed_tag(nCapability, CT_SERVER_FLAGS, true));
        m_login.m_list.add_tag(make_typed_tag(nClientVersion, CT_EMULE_VERSION, true));
        m_login.m_list.dump();


        // after connect write message to server
        m_conn.async_write(m_login, boost::bind(&client::handle_write, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

    void handle_write(const boost::system::error_code& error, size_t nSize)
    {
        // after wrote data read answer
        if (!error)
        {
            LDBG_ << "Write completed ";
            libed2k_header h;

/*
            try
            {
                m_conn.socket().read_some(boost::asio::buffer(&h, sizeof(h)));
                LDBG_ << "read packet header " << packetToString(h.m_type);
            }
           catch(boost::system::error_code& err)
           {
               LDBG_ << "error on read header " << err ;
           }
*/
            //m_conn.async_read_header(boost::bind(&client::handle_read_header, this, boost::asio::placeholders::error));
            m_conn.async_read();
        }
        else
        {
            LERR_ << "write error " << error.message();
            do_close(error);
        }
    }

    void on_read_server_message(base_socket::socket_buffer& sb, const boost::system::error_code& error)
    {
        LDBG_ << "read server message";
        LDBG_ << "Read header " << packetToString(m_conn.context().m_type);

        if (!error)
        {
            boost::iostreams::stream_buffer<base_socket::Device> buffer(&sb[0], m_conn.context().m_size - 1);
            std::istream in_array_stream(&buffer);
            archive::ed2k_iarchive ia(in_array_stream);
            server_message sm;
            ia >> sm;
            m_conn.async_read();    // read next messages
        }
    }

    void on_read_unhandled_message(base_socket::socket_buffer& sb, const boost::system::error_code& error)
    {
        LDBG_ << "client receive unhandled packet" << packetToString(m_conn.context().m_type);
        m_conn.async_read();
    }

    void do_close(const boost::system::error_code& error)
    {
        LDBG_ << error.message();
        m_conn.socket().close();
    }

    cs_login_request    m_login;
    md4_hash            m_hLocal;
    server_message      m_message;
};


}

using namespace libed2k;

int main(int argc, char* argv[])
{
    init_logs();

    LDBG_ << "size of header " << sizeof(libed2k_header);
    LAPP_ << " first output";
    BOOST_SCOPED_LOG_CTX(LAPP_) << " main";

    boost::asio::io_service io;
    // our server endpoint

    //boost::asio::ip::tcp::endpoint server_ep("dore", 4662);

    boost::asio::ip::tcp::endpoint server_point(tcp::v4(), 4662);
    nAddr = server_point.address().to_v4().to_ulong();
    LDBG_ << "our ulong id: " << nAddr;

    // ok, prepare end point
    tcp::resolver resolver(io);
    tcp::resolver::query query("che-d-113", "4661");
    tcp::resolver::iterator iterator = resolver.resolve(query); // use async resolve

    // eMule server end point
    boost::asio::ip::tcp::endpoint client_point = *iterator;
    tcp_server tserver(io, server_point);
    client c(io, client_point);
    io.run();


    return 0;
}

