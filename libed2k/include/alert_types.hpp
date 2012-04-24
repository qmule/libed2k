#ifndef __LIBED2K_ALERT_TYPES__
#define __LIBED2K_ALERT_TYPES__

// for print_endpoint
#include <libtorrent/socket.hpp>

#include "alert.hpp"
#include "types.hpp"
#include "error_code.hpp"


namespace libed2k
{
    /**
      * emit when server connection was initialized
     */
    struct a_server_connection_initialized : alert
    {
        const static int static_category = alert::status_notification;

        a_server_connection_initialized(boost::uint32_t nClientId, boost::uint32_t nFilesCount, boost::uint32_t nUsersCount) :
            m_nClientId(nClientId), m_nFilesCount(nFilesCount), m_nUsersCount(nUsersCount)
        {}
        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new a_server_connection_initialized(*this));
        }

        virtual std::string message() const { return std::string("server connection was initialized"); }
        virtual char const* what() const { return "server notification"; }

        boost::uint32_t m_nClientId;
        boost::uint32_t m_nFilesCount;
        boost::uint32_t m_nUsersCount;
    };

    /**
      * emit for every server message
     */
    struct a_server_message: alert
    {
        const static int static_category = alert::server_notification;

        a_server_message(const std::string& strMessage) : m_strMessage(strMessage){}
        virtual int category() const { return static_category; }

        virtual std::string message() const { return m_strMessage; }
        virtual char const* what() const { return "server message incoming"; }

        virtual std::auto_ptr<alert> clone() const
        {
            return (std::auto_ptr<alert>(new a_server_message(*this)));
        }

        std::string m_strMessage;
    };

    struct a_listen_failed_alert: alert
    {
        a_listen_failed_alert(tcp::endpoint const& ep, error_code const& ec):
            endpoint(ep), error(ec)
        {}

        tcp::endpoint endpoint;
        error_code error;

        virtual std::auto_ptr<alert> clone() const
        { return std::auto_ptr<alert>(new a_listen_failed_alert(*this)); }
        virtual char const* what() const { return "listen failed"; }
        const static int static_category = alert::status_notification | alert::error_notification;
        virtual int category() const { return static_category; }
        virtual std::string message() const
        {
            char ret[200];
            snprintf(ret, sizeof(ret), "listening on %s failed: %s"
                , libtorrent::print_endpoint(endpoint).c_str(), error.message().c_str());
            return ret;
        }
    };

}


#endif //__LIBED2K_ALERT_TYPES__
