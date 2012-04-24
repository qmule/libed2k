#ifndef __LIBED2K_ALERT_TYPES__
#define __LIBED2K_ALERT_TYPES__

#include "alert.hpp"

namespace libed2k
{
    struct server_alert: alert
    {
        const static int static_category = alert::server_notification;

        server_alert(boost::uint32_t nClientId) : m_nClientId(nClientId)
        {}
        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new server_alert(*this));
        }

        virtual std::string message() const { return std::string(""); }
        virtual char const* what() const { return "server notification"; }
        boost::uint32_t m_nClientId;
    };

}


#endif //__LIBED2K_ALERT_TYPES__
