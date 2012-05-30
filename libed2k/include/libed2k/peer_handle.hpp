#ifndef __PEER_HANDLE__
#define __PEER_HANDLE__

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

namespace libed2k
{
    class peer_connection;
    struct misc_options;
    struct misc_options2;


    namespace aux
    {
        class session_impl_base;
        class session_impl;
        class session_impl_test;
    }

    struct peer_handle
    {
        friend class aux::session_impl_base;
        friend class aux::session_impl;
        friend class aux::session_impl_test;
        friend class peer_connection;

        peer_handle();

        void send_message(const std::string& strMessage);
        void get_shared_files() const;
        void get_shared_directories() const;
        void get_shared_directory_files(const std::string& strDirectory) const;

        misc_options get_misc_options() const;
        misc_options2 get_misc_options2() const;

        std::string get_name() const;
        int get_version() const;
        int get_port() const;
        bool is_active() const;

        bool operator==(const peer_handle& ph) const
        { return m_peer.lock() == ph.m_peer.lock(); }

        bool operator!=(const peer_handle& ph) const
        { return m_peer.lock() != ph.m_peer.lock(); }

        bool operator<(const peer_handle& ph) const
        { return m_peer.lock() < ph.m_peer.lock(); }
    private:
        peer_handle(const boost::weak_ptr<peer_connection>& t):
            m_peer(t)
        {}

        boost::weak_ptr<peer_connection> m_peer;
    };

}


#endif //__PEER_HANDLE__
