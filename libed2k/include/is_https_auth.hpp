#ifndef __IS_HTTP_AUTH__
#define __IS_HTTP_AUTH__

#include <sstream>
#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/asio/ssl.hpp>
#include "types.hpp"


namespace libed2k{

namespace ssl = boost::asio::ssl;

/**
  * this class provide authorization to is https server
  *
*/
class is_https_auth : boost::noncopyable
{
public:
    //перечисление содержит статус авторизации
    enum eAuthStatus
    {
        AUTH_NONE = 0,
        AUTH_SUCCESS,
        AUTH_STARTED,
        AUTH_REQUEST,
        AUTH_WAITING,
        AUTH_FAILED
    };

    //перечисление содержит типы сообщений авторизации
    enum eAuthMessageType
    {
        LOG_MESSAGE = 0,
        INFO_MESSAGE_NOMODAL,
        INFO_MESSAGE_MODAL,
        ERR_MESSAGE,
        WARN_PROTOCOL,
        ERR_PROTOCOL
    };

    // перечисление содержит константы результата авторизации
    enum eAuthResult
    {
        AUTH_OK = 0,
        AUTH_ERROR,
        AUTH_WAIT
    };

    typedef boost::function<void (std::string&, const boost::system::error_code&)> auth_callback;

	explicit is_https_auth(boost::asio::io_service& service, auth_callback on_auth = NULL);

	void requestAuth(const std::string& strHost,
					const std::string& strPage,
					const std::string& strLogin,
					const std::string& strPassword,
					const std::string& strVersion);	
private:
    /**
      * async resolve callback
     */
	void handle_resolve(const boost::system::error_code& error, tcp::resolver::iterator itr);	

	template <typename Iterator>
	Iterator conn_condition(const boost::system::error_code& ec,  Iterator next)
	{
	    //if (ec) std::cout << "Error: " << ec.message() << std::endl;
	    //std::cout << "Trying: " << next->endpoint() << std::endl;
	    return next;
	}

	/**
	  *  async connect callback
	*/
	void handle_connect(const boost::system::error_code& error);

    /**
      * async write callback
    */
	void handle_write_request(const boost::system::error_code& error, size_t);

    /**
      * async http first line request received callback
     */
	void handle_response_header(const boost::system::error_code& error);

    /**
      * async http header reseived callback
     */
	void handle_response_body(const boost::system::error_code& error);

    /**
      * async http body message received callback
     */
	void handle_data(const boost::system::error_code& error);

    /**
      * async error handle callback
     */
	void handle_error(const boost::system::error_code& error);


	boost::asio::io_service& 	m_service;  //!< service object for async operations
	ssl::context				m_context;  //!< openssl context
	ssl::stream<tcp::socket>	m_ssec;     //!< secure socket
	tcp::resolver				m_resolver; //!< tcp resolver
	boost::asio::streambuf		m_response; //!< response buffer
	boost::asio::streambuf 		m_request;	//!< request buffer
	auth_callback               m_on_auth;
	std::string                 m_strResult; //XML result
};

}

#endif //__IS_HTTP_AUTH__
