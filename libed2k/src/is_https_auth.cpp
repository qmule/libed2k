#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>
#include "is_https_auth.hpp"

using namespace libed2k;

is_https_auth::is_https_auth(boost::asio::io_service& service, auth_callback on_auth /* = NULL*/) :
		m_service(service),
		m_context(ssl::context::sslv23),
		m_ssec(service, m_context),
		m_resolver(service),
		m_response(1024), // read only 1024 bytes length,
        m_on_auth(on_auth)
{

}

void is_https_auth::requestAuth(const std::string& strHost,
    const std::string& strPage,
    const std::string& strLogin,
    const std::string& strPassword,
    const std::string& strVersion)
{
    using namespace boost::archive::iterators;
    m_strResult.clear();
    typedef insert_linebreaks<base64_from_binary<transform_width<const char *,6,8> >,72> base64_text;

    // generate base64 post data
    std::string strPostData = "login=";
    std::copy(base64_text(strLogin.c_str()), base64_text(strLogin.c_str() + strLogin.size()), std::inserter(strPostData, strPostData.end()));
    strPostData += "&password=";
    std::copy(base64_text(strPassword.c_str()), base64_text(strPassword.c_str() + strPassword.size()), std::inserter(strPostData, strPostData.end()));
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

    // resolve 
    m_resolver.async_resolve(tcp::resolver::query(strHost, "https"), boost::bind(&is_https_auth::handle_resolve, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::iterator));
}


void is_https_auth::handle_resolve(const boost::system::error_code& error, tcp::resolver::iterator itr)
{
    if (!error)
    {
        // prepare to connect
        boost::asio::async_connect(m_ssec.lowest_layer(), itr, boost::bind(&is_https_auth::conn_condition<tcp::resolver::iterator>, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::iterator), boost::bind(&is_https_auth::handle_connect, this, boost::asio::placeholders::error));
    }
    else
    {
        handle_error(error);
    }
}

void is_https_auth::handle_connect(const boost::system::error_code& error)
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
        boost::asio::async_write(m_ssec, m_request, boost::bind(&is_https_auth::handle_write_request, this,
            boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

    }
    else
    {
        handle_error(error);
    }
}

void is_https_auth::handle_write_request(const boost::system::error_code& error, size_t)
{
    if (!error)
    {			
        // read response header at least
        boost::asio::async_read_until(m_ssec, m_response, "\r\n", boost::bind(&is_https_auth::handle_response_header, this,
            boost::asio::placeholders::error));
    }
    else
    {
        handle_error(error);
    }

}

void is_https_auth::handle_response_header(const boost::system::error_code& error)
{
    if (!error)
    {			
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
        boost::asio::async_read_until(m_ssec, m_response, "\r\n\r\n", boost::bind(&is_https_auth::handle_response_body, this,
            boost::asio::placeholders::error));
    }
    else
    {
        handle_error(error);
    }
}

void is_https_auth::handle_response_body(const boost::system::error_code& error)
{
    if (!error)
    {			
        std::istream response_stream(&m_response);
        std::string header;

        // roll to empty line
        while (std::getline(response_stream, header) && header != "\r")
        {
            //std::cout << header << "\n";
        }

        boost::asio::async_read_until(m_ssec, m_response, "</DATA>",
            boost::bind(&is_https_auth::handle_data, this, boost::asio::placeholders::error));
    }
    else
    {
        handle_error(error);
    }
}

void is_https_auth::handle_data(const boost::system::error_code& error)
{
    if (!error)
    {
        std::istream response_stream(&m_response);
        std::string strLine;

        while (std::getline(response_stream, strLine))
        {
            m_strResult += strLine;
            if (strLine.find_first_of("</DATA>") != std::string::npos)
            {
                break;
            }

        }
    }

    handle_error(error);
}

void is_https_auth::handle_error(const boost::system::error_code& error)
{
    m_on_auth(m_strResult, error);
    m_ssec.lowest_layer().close();
}

