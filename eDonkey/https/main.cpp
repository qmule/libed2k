// https.cpp : Defines the entry point for the console application.
//
#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/asio/ssl.hpp>


using boost::asio::ip::tcp;
using namespace std;

int main(int argc, char* argv[])
{
	try
	{
		using boost::asio::ip::tcp;
		namespace ssl = boost::asio::ssl;
		typedef ssl::stream<tcp::socket> ssl_socket;

		// Create a context that uses the default paths for
		// finding CA certificates.
		ssl::context ctx(ssl::context::sslv23);
		///io_service, boost::asio::ssl::context::sslv23);
		//ctx.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2);
		ctx.set_default_verify_paths();

		// Open a socket and connect it to the remote host.
		boost::asio::io_service io_service;
		ssl_socket sock(io_service, ctx);
		tcp::resolver resolver(io_service);
		tcp::resolver::query query("el.is74.ru", "https");
		boost::asio::connect(sock.lowest_layer(), resolver.resolve(query));
		sock.lowest_layer().set_option(tcp::no_delay(true));

		// Perform SSL handshake and verify the remote host's
		// certificate.
		sock.set_verify_mode(ssl::verify_none);
		sock.set_verify_callback(ssl::rfc2818_verification("el.is74.ru"));
		sock.handshake(ssl_socket::client);
		
		boost::asio::streambuf request;
	    std::ostream request_stream(&request);
		//request_stream << "GET " << argv[2] << " HTTP/1.0\r\n";
		//request_stream << "Host: " << argv[1] << "\r\n";
		//request_stream << "Accept: */*\r\n";
		//request_stream << "Connection: close\r\n\r\n";

		string strLogin = "login=pavlovatg74&password=&nocrypt=yes";
		request_stream << "POST /auth.php HTTP/1.0\r\n";
		request_stream << "Host: el.is74.ru\r\n";
		request_stream << "Connection: close\r\n";
		request_stream << "Content-Type: application/x-www-form-urlencoded\r\n";
		request_stream << "Content-Length: " << strLogin.size() << "\r\n";
		request_stream << "\r\n";
		request_stream << strLogin << "\r\n\r\n";

		// Send the request.
		boost::asio::write(sock, request);

		boost::asio::streambuf response;
	    boost::asio::read_until(sock, response, "\r\n");

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
		boost::asio::read_until(sock, response, "\r\n\r\n");

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
		while (boost::asio::read(sock, response,
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

