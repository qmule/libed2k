// https.cpp : Defines the entry point for the console application.
//
#include <sstream>
#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>


using boost::asio::ip::tcp;
using namespace std;

// Read answer
boost::asio::streambuf response(12);

using boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

struct my_connect_condition
{
  template <typename Iterator>
  Iterator operator()(
      const boost::system::error_code& ec,
      Iterator next)
  {
    if (ec) std::cout << "Error: " << ec.message() << std::endl;
    std::cout << "Trying: " << next->endpoint() << std::endl;
    return next;
  }
};

class IsAuth
{
public:

	IsAuth(boost::asio::io_service& service) :
		m_service(service),
		m_context(ssl::context::sslv23),
		m_ssec(service, m_context),
		m_resolver(service)
	{

	}

	void requestAuth(const std::string& strHost,
					const std::string& strPage,
					const std::string& strLogin,
					const std::string& strPassword,
					const std::string& strVersion)
	{
		using namespace boost::archive::iterators;
		typedef insert_linebreaks<base64_from_binary<transform_width<const char *,6,8> >,72> base64_text;

		// generate base64 post data
		string strPostData = "login=";
		std::copy(base64_text(strLogin.c_str()), base64_text(strLogin.c_str() + strLogin.size()), std::inserter(strPostData, strPostData.end()));
		strPostData += "&password=";
		std::copy(base64_text(strPassword.c_str()), base64_text(strPassword.c_str() + strPassword.size()), std::inserter(strPostData, strPostData.end()));
		//m_strPostData += "&nick=";
		//std::copy(base64_text(strNick.c_str()), base64_text(strNick.c_str() + strNick.size()), std::inserter(strContent, strContent.end()));
		strPostData += "&version=";
		std::copy(base64_text(strVersion.c_str()), base64_text(strVersion.c_str() + strVersion.size()), std::inserter(strPostData, strPostData.end()));

		// prepare request
		std::ostream request_stream(&m_request);
		request_stream << "POST /" << strPage << " HTTP/1.0\r\n";
		request_stream << "User-Agent: eMule IS Mod\r\n";
		request_stream << "Host: " << strHost << "\r\n";
		request_stream << "Accept: */*\r\n";
		request_stream << "Connection: close\r\n";
		request_stream << "Content-Type: application/x-www-form-urlencoded\r\n";
		request_stream << "Content-Length: " << strPostData.length();
		request_stream	<< "\r\n\r\n" << strPostData;
		request_stream << "\r\n\r\n";

		if (m_ssec.lowest_layer().is_open())
		{
			m_ssec.lowest_layer().close();
		}

		m_context.set_default_verify_paths();

		m_resolver.async_resolve(tcp::resolver::query(strHost, "https"), boost::bind(&IsAuth::handle_resolve, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::iterator));
	}
private:
	void handle_resolve(const boost::system::error_code& error, tcp::resolver::iterator itr)
	{
		if (!error)
		{
			// prepare to connect
			boost::asio::async_connect(m_ssec.lowest_layer(), itr, boost::bind(&IsAuth::conn_condition<tcp::resolver::iterator>, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::iterator), boost::bind(&IsAuth::handle_connect, this, boost::asio::placeholders::error));
		}
	}

	template <typename Iterator>
	Iterator conn_condition(const boost::system::error_code& ec,  Iterator next)
	{
	    if (ec) std::cout << "Error: " << ec.message() << std::endl;

	    std::cout << "Trying: " << next->endpoint() << std::endl;

	    return next;
	}

	void handle_connect(const boost::system::error_code& error)
	{
		if (!error)
		{
			// execute handshake
			m_ssec.lowest_layer().set_option(tcp::no_delay(true));

			// Perform SSL handshake and verify the remote host's
			// certificate.
			m_ssec.set_verify_mode(ssl::verify_none);
			m_ssec.set_verify_callback(ssl::rfc2818_verification("el.is74.ru"));
			m_ssec.handshake(ssl::stream<tcp::socket>::client);
			boost::asio::async_write(m_ssec, m_request, boost::bind(&IsAuth::handle_write_request, this,
					boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

		}
		else
		{
			std::cout << "error " << error.message() << std::endl;
		}
	}

	void handle_write_request(const boost::system::error_code& error, size_t)
	{
		if (!error)
		{
			std::cout << "write request " << std::endl;
			// read response header at least
			boost::asio::async_read_until(m_ssec, m_response, "\r\n", boost::bind(&IsAuth::handle_response_header, this,
					boost::asio::placeholders::error));
		}
	}

	void handle_response_header(const boost::system::error_code& error)
	{
		if (!error)
		{
			std::cout << "header ready \n";
			// Check that response is OK.
			std::istream response_stream(&m_response);

			std::string http_version;
			unsigned int status_code;
			std::string status_message;

			response_stream >> http_version;
			response_stream >> status_code;

			std::getline(response_stream, status_message);

			if (!response_stream || http_version.substr(0, 5) != "HTTP/")
			{
			  std::cout << "Invalid response\n";
			  return;
			}

			if (status_code != 200)
			{
			  std::cout << "Response returned with status code " << status_code << "\n";
			  return;
			}

			// read response header at least
			boost::asio::async_read_until(m_ssec, m_response, "\r\n\r\n", boost::bind(&IsAuth::handle_response_body, this,
							boost::asio::placeholders::error));
		}
		else
		{
			std::cout << error.message() << std::endl;
		}
	}

	void handle_response_body(const boost::system::error_code& error)
	{
		if (!error)
		{
			std::cout << "body ready\n";
			std::istream response_stream(&m_response);
			std::string header;
			while (std::getline(response_stream, header) && header != "\r")
			  std::cout << header << "\n";
			std::cout << "\n";

			boost::asio::async_read_until(m_ssec, m_response, "</DATA>",
					boost::bind(&IsAuth::handle_data, this, boost::asio::placeholders::error));
		}
	}

	void handle_data(const boost::system::error_code& error)
	{
		if (!error)
		{
			std::string strLine;
			std::istream response_stream(&m_response);
			while (std::getline(response_stream, strLine))
			{
				std::cout << "LINE: " << strLine << "\n";
			}

		}
		else
		{/*
			if (error != boost::asio::error::eof) throw boost::system::system_error(error);

			std::string strLine;
			std::istream response_stream(&m_response);

			while (std::getline(response_stream, strLine))
			{
				std::cout << strLine << "\n";
			}

			std::cout << "All readed\n";
			*/
		}
	}

	boost::asio::io_service& 	m_service;
	ssl::context				m_context;
	ssl::stream<tcp::socket>	m_ssec;
	tcp::resolver				m_resolver;
	boost::asio::streambuf 		m_request;
	boost::asio::streambuf		m_response;

};

int main(int argc, char* argv[])
{
	try
	{

		boost::asio::io_service io_service;
		IsAuth auth(io_service);
		auth.requestAuth("el.is74.ru", "auth.php", "pavlovatg74", "", "0.5.6.7");
		io_service.run();
		return 0;
/*
		using namespace boost::archive::iterators;


		// base64 transform
		// b bits to 6 bits -> encode to base64 -> add line break
		typedef insert_linebreaks<base64_from_binary<transform_width<const char *,6,8> >,72> base64_text; // compose all the above operations in to a new iterator

		std::string strLogin    = "pavlovatg74";
		std::string strPassword = "";
		std::string strNick 	= "pavlovatg74";
		std::string strVer      = "0.26.2.5";

		string strContent = "login=";
		std::copy(base64_text(strLogin.c_str()), base64_text(strLogin.c_str() + strLogin.size()), std::inserter(strContent, strContent.end()));
		strContent += "&password=";
		std::copy(base64_text(strPassword.c_str()), base64_text(strPassword.c_str() + strPassword.size()), std::inserter(strContent, strContent.end()));
		strContent += "&nick=";
		std::copy(base64_text(strNick.c_str()), base64_text(strNick.c_str() + strNick.size()), std::inserter(strContent, strContent.end()));
		strContent += "&version=";
		std::copy(base64_text(strVer.c_str()), base64_text(strVer.c_str() + strVer.size()), std::inserter(strContent, strContent.end()));
		std::cout << strContent << std::endl;

		using boost::asio::ip::tcp;
		namespace ssl = boost::asio::ssl;
		typedef ssl::stream<tcp::socket> ssl_socket;
		tcp::resolver resolver(io_service);
		tcp::resolver::query query("el.is74.ru", "https");

		// Create a context that uses the default paths for
		// finding CA certificates.
		ssl::context ctx(ssl::context::sslv23);
		ctx.set_default_verify_paths();

		// Open a socket and connect it to the remote host.
		ssl_socket sock(io_service, ctx);


		boost::asio::connect(sock.lowest_layer(), resolver.resolve(query));
		sock.lowest_layer().set_option(tcp::no_delay(true));

		// Perform SSL handshake and verify the remote host's
		// certificate.
		sock.set_verify_mode(ssl::verify_none);
		sock.set_verify_callback(ssl::rfc2818_verification("el.is74.ru"));
		sock.handshake(ssl_socket::client);
		
		boost::asio::streambuf request;
	    std::ostream request_stream(&request);
		//size_t nLength = string("login=&password=&nick=").size() + osLogin.str().size() + osPassword.str().size() + osNick.str().size();
		strLogin = "login=pavlovatg74&password=&nocrypt=yes";
		request_stream << "POST /auth.php HTTP/1.0\r\n";
		request_stream << "User-Agent: eMule IS Mod\r\n";
		request_stream << "Host: el.is74.ru\r\n";

		request_stream << "Connection: close\r\n";
		request_stream << "Content-Type: application/x-www-form-urlencoded\r\n";
		request_stream << "Content-Length: " << strContent.length();
		request_stream	<< "\r\n\r\n" << strContent;
		request_stream << "\r\n\r\n";


		// Send the request.
		boost::asio::write(sock, request);

	    boost::asio::read_until(sock, response, "\r\n");

		// Check that response is OK.
		std::istream response_stream(&response);
		
		std::string http_version;
		unsigned int status_code;
		std::string status_message;

		response_stream >> http_version;
		response_stream >> status_code;

		std::getline(response_stream, status_message);

		if (!response_stream || http_version.substr(0, 5) != "HTTP/")
		{
		  std::cout << "Invalid response\n";
		  return 1;
		}

		if (status_code != 200)
		{
		  std::cout << "Response returned with status code " << status_code << "\n";
		  return 1;
		}




		// Read the response headers, which are terminated by a blank line.
		boost::asio::read_until(sock, response, "\r\n\r\n");

		std::cout << "Read to blank " << std::endl;
		// Process the response headers.
		std::string header;
		while (std::getline(response_stream, header) && header != "\r")
		  std::cout << header << "\n";
		std::cout << "\n";

		// Write whatever content we already have to output.
		if (response.size() > 0)
		{
			if (std::getline(response_stream, header))
			{
				std::cout << "XML: " << header << std::endl;
			}
		}


		// Read until EOF, writing data to output as we go.
		boost::system::error_code error;
		while (boost::asio::read(sock, response, boost::asio::transfer_at_least(1), error))
		{

		}

		if (error != boost::asio::error::eof) throw boost::system::system_error(error);
		*/
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

  return 0;
}

/*
int main(int argc, char* argv[])
{
	try
	{
		if (argc != 3)
		{
			std::cerr << "Usage: client <host>" << std::endl;
			return 1;
		}

		ssl::context ctx(ssl::context::sslv23);
		//ctx.set_verify_mode(ssl::verify_peer);

		cout << "work on " << argv[1] << endl;
		boost::asio::io_service io_service;
		tcp::resolver resolver(io_service);
		tcp::resolver::query query(argv[1], "https");
		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		//tcp::endpoint the_client_endpoint; // = *endpoint_iterator;


		//tcp::endpoint the_client_endpoint;
		//the_client_endpoint.port(8080);
		//the_client_endpoint.address(boost:asio::ip::address_v4::from_string("127.0.0.1"));
		//endpoint_iterator->endpoint().port(8080);
		tcp::socket socket(io_service, ctx);
		boost::asio::connect(socket, endpoint_iterator);

		boost::asio::streambuf request;
	    std::ostream request_stream(&request);
		request_stream << "GET " << argv[2] << " HTTPS/1.0\r\n";
		request_stream << "Host: " << argv[1] << "\r\n";
		request_stream << "Accept: *\/*\r\n";
		request_stream << "Connection: close\r\n\r\n";

		// Send the request.
		boost::asio::write(socket, request);

		boost::asio::streambuf response;
	    boost::asio::read_until(socket, response, "\r\n");

		// Check that response is OK.
		std::istream response_stream(&response);
		std::string http_version;
		response_stream >> http_version;
		unsigned int status_code;
		response_stream >> status_code;
		std::string status_message;

		std::getline(response_stream, status_message);

		if (!response_stream || http_version.substr(0, 5) != "HTTP/")
		{
		  std::cout << "Invalid response\n";
		  return 1;
		}

		if (status_code != 200)
		{
		  std::cout << "Response returned with status code " << status_code << "\n";
		  return 1;
		}

		// Read the response headers, which are terminated by a blank line.
		boost::asio::read_until(socket, response, "\r\n\r\n");

		// Process the response headers.
		std::string header;
		while (std::getline(response_stream, header) && header != "\r")
		  std::cout << header << "\n";
		std::cout << "\n";

		// Write whatever content we already have to output.
		if (response.size() > 0)
		  std::cout << &response;

		// Read until EOF, writing data to output as we go.
		boost::system::error_code error;
		while (boost::asio::read(socket, response,
			  boost::asio::transfer_at_least(1), error))
		  std::cout << &response;
		if (error != boost::asio::error::eof)
		  throw boost::system::system_error(error);		
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
*/

