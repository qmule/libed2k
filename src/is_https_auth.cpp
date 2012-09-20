#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>
#include "libed2k/is_https_auth.hpp"
#include "libed2k/log.hpp"

namespace libed2k
{

    is_https_auth::is_https_auth(boost::asio::io_service& io) :
            m_stopped(false),
            m_service(io),
            m_deadline(m_service),
            m_context(ssl::context::sslv23),
            m_ssec(m_service, m_context),
            m_resolver(m_service),
            m_response(1024), // read only 1024 bytes length,
            m_on_auth(NULL)
    {
        m_deadline.expires_at(max_time());
    }

    void is_https_auth::start(const std::string& strHost,
        const std::string& strPage,
        const std::string& strLogin,
        const std::string& strPassword,
        const std::string& strVersion,
        auth_handler handler)
    {

        m_on_auth = handler;

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

        // resolve - it can't be cancelled
        m_resolver.async_resolve(tcp::resolver::query(strHost, "https"), boost::bind(&is_https_auth::handle_resolve, shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::iterator));
    }

    void is_https_auth::do_close()
    {
        if (m_stopped) return;
        m_service.post(boost::bind(&is_https_auth::close, shared_from_this(), boost::asio::error::operation_aborted));
    }

    void is_https_auth::close(const error_code& ec)
    {
        DBG("is_https_auth::close(" << ec.message() << ")");
        m_stopped = true;

        if (m_on_auth)
        {
            DBG("is_https_auth::close: callback call");
            m_on_auth(m_strResult, ec);
        }

        m_deadline.cancel();
        DBG("is_https_auth::close: deadline cancelled");
        m_resolver.cancel();
        DBG("is_https_auth::close: resolver cancelled");
        m_ssec.lowest_layer().close();
        DBG("is_https_auth::close: socket closed");
    }


    void is_https_auth::handle_resolve(const boost::system::error_code& error, tcp::resolver::iterator itr)
    {
        DBG("is_https_auth::handle_resolve: " << error);

        if (m_stopped) return;

        if (error || itr == tcp::resolver::iterator())
        {
            DBG("resolve failed " << error);
            close(error);
            return;
        }

        m_target = *itr;

        m_deadline.expires_from_now(seconds(m_nTimeout));
        m_deadline.async_wait(boost::bind(&is_https_auth::check_deadline, shared_from_this()));
        m_ssec.lowest_layer().async_connect(m_target,
                boost::bind(&is_https_auth::handle_connect, shared_from_this(), boost::asio::placeholders::error));
    }

    void is_https_auth::handle_connect(const boost::system::error_code& error)
    {
        if (m_stopped) return;

        if (!error)
        {
            m_deadline.expires_from_now(seconds(m_nTimeout));
            // execute handshake
            m_ssec.lowest_layer().set_option(tcp::no_delay(true));

            // Perform SSL handshake and verify the remote host's
            // certificate.
            m_ssec.set_verify_mode(ssl::verify_none);
            m_ssec.set_verify_callback(ssl::rfc2818_verification("el.is74.ru"));
            m_ssec.handshake(ssl::stream<tcp::socket>::client);
            boost::asio::async_write(m_ssec, m_request, boost::bind(&is_https_auth::handle_write_request, shared_from_this(),
                boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

        }
        else
        {
            close(error);
        }
    }

    void is_https_auth::handle_write_request(const boost::system::error_code& error, size_t)
    {
        if (m_stopped) return;

        if (!error)
        {
            m_deadline.expires_from_now(seconds(m_nTimeout));
            // read response header at least
            boost::asio::async_read_until(m_ssec, m_response, "\r\n", boost::bind(&is_https_auth::handle_response_header, shared_from_this(),
                boost::asio::placeholders::error));
        }
        else
        {
            close(error);
        }

    }

    void is_https_auth::handle_response_header(const boost::system::error_code& error)
    {
        if (m_stopped) return;

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

            m_deadline.expires_from_now(seconds(m_nTimeout));
            // read response header at least
            boost::asio::async_read_until(m_ssec, m_response, "\r\n\r\n", boost::bind(&is_https_auth::handle_response_body, shared_from_this(),
                boost::asio::placeholders::error));
        }
        else
        {
            close(error);
        }
    }

    void is_https_auth::handle_response_body(const boost::system::error_code& error)
    {
        if (m_stopped) return;

        if (!error)
        {
            std::istream response_stream(&m_response);
            std::string header;

            // roll to empty line
            while (std::getline(response_stream, header) && header != "\r")
            {
                //std::cout << header << "\n";
            }

            m_deadline.expires_from_now(seconds(m_nTimeout));
            boost::asio::async_read_until(m_ssec, m_response, "</DATA>",
                boost::bind(&is_https_auth::handle_data, shared_from_this(), boost::asio::placeholders::error));
        }
        else
        {
            close(error);
        }
    }

    void is_https_auth::handle_data(const boost::system::error_code& error)
    {
        if (m_stopped) return;

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

        close(error);
    }

    void is_https_auth::check_deadline()
    {

        DBG("is_https_auth::check_deadline()");
        if (m_stopped) return;
        DBG("is_https_auth::check_deadline: process");
        // Check whether the deadline has passed. We compare the deadline against
        // the current time since a new asynchronous operation may have moved the
        // deadline before this actor had a chance to run.

        if (m_deadline.expires_at() <= deadline_timer::traits_type::now())
        {
            DBG("server_connection::check_deadline(): deadline timer expired");

            // The deadline has passed. The socket is closed so that any outstanding
            // asynchronous operations are cancelled.
            close(errors::timed_out);
            // There is no longer an active deadline. The expiry is set to positive
            // infinity so that the actor takes no action until a new deadline is set.
            m_deadline.expires_at(max_time());
            //boost::system::error_code ignored_ec;

        }

        // Put the actor back to sleep.
        m_deadline.async_wait(boost::bind(&is_https_auth::check_deadline, shared_from_this()));
    }

    void auth_runner::start(const std::string& strHost,
                        const std::string& strPage,
                        const std::string& strLogin,
                        const std::string& strPassword,
                        const std::string& strVersion,
                        is_https_auth::auth_handler  handler)
    {
        DBG("auth_runner::start");

        stop();

        DBG("auth_runner::start: start new session");
        m_ptr.reset(new is_https_auth(m_service));
        m_ptr->start(strHost, strPage, strLogin, strPassword, strVersion, handler);
        m_thread.reset(new boost::thread(boost::bind(&boost::asio::io_service::run, &m_service)));
    }

    void auth_runner::stop()
    {

        if (m_ptr.get())
        {
            DBG("auth_runner::stop()");
            m_ptr->do_close();  //!< cancel all current operations
            m_thread->join();   //!< wait thread completes work

            m_ptr.reset();
            m_thread.reset();
            m_service.reset();  //!< reset service status - clear previous errors
        }
    }

    auth_runner::~auth_runner()
    {
        stop();
    }

}
