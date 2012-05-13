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
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include "libed2k/error_code.hpp"
#include "libed2k/types.hpp"


namespace libed2k
{

    namespace ssl = boost::asio::ssl;

    class auth_runner;

    /**
      * this class provide single authorization call to IS https server
      *
    */
    class is_https_auth : public boost::noncopyable, public boost::enable_shared_from_this<is_https_auth>
    {
    public:
        static const int m_nTimeout = 10;

        typedef boost::function<void (const std::string&, const error_code&)> auth_handler;

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

        explicit is_https_auth(boost::asio::io_service& io);

        void start(const std::string& strHost,
                        const std::string& strPage,
                        const std::string& strLogin,
                        const std::string& strPassword,
                        const std::string& strVersion,
                        auth_handler  handler);

        void do_close();

    private:

        /**
          * stop auth request
         */
        void close(const error_code& ec);

        /**
          * async resolve callback
         */
        void handle_resolve(const boost::system::error_code& error, tcp::resolver::iterator itr);

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

        void check_deadline();

        bool                        m_stopped;
        boost::asio::io_service&    m_service;
        dtimer                      m_deadline;
        ssl::context				m_context;  //!< openssl context
        ssl::stream<tcp::socket>	m_ssec;     //!< secure socket
        tcp::resolver				m_resolver; //!< tcp resolver
        boost::asio::streambuf		m_response; //!< response buffer
        boost::asio::streambuf 		m_request;	//!< request buffer
        auth_callback               m_on_auth;
        std::string                 m_strResult; //XML result
        tcp::endpoint               m_target;
    };

    /**
      * this class need for run several auth calls in dedicated thread without
      * blocking main thread
      *
     */
    class auth_runner
    {
    public:
        friend class is_https_auth;

        void start(const std::string& strHost,
                            const std::string& strPage,
                            const std::string& strLogin,
                            const std::string& strPassword,
                            const std::string& strVersion,
                            is_https_auth::auth_handler  handler);

        void stop();

        ~auth_runner();
    private:
        boost::asio::io_service             m_service;
        boost::shared_ptr<is_https_auth>    m_ptr;
        boost::shared_ptr<boost::thread>    m_thread;
    };

}

#endif //__IS_HTTP_AUTH__
