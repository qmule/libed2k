#ifndef __PEER_HANDLE__
#define __PEER_HANDLE__

#include <string>
#include <boost/intrusive_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include "libed2k/packet_struct.hpp"


namespace libed2k
{
    class peer_connection;
    struct misc_options;
    struct misc_options2;
    struct net_identifier;

    namespace aux
    {
        class session_impl_base;
        class session_impl;
        class session_impl_test;
    }

    struct peer_connection_handle
    {
        friend class aux::session_impl_base;
        friend class aux::session_impl;
        friend class aux::session_impl_test;
        friend class peer_connection;

        peer_connection_handle();

        /**
          * these methods send meta packets to send order
          * we use io_service.post for these call for call order modification
          * from session_impl thread
         */
        void send_message(const std::string& strMessage);
        void get_shared_files() const;
        void get_shared_directories() const;
        void get_shared_directory_files(const std::string& strDirectory) const;
        void get_ismod_directory(const md4_hash& hash) const;

        /**
          * lock mutex calls - simple return internal object state
         */
        md4_hash                get_hash() const;
        net_identifier          get_network_point() const;
        peer_connection_options get_options() const;
        misc_options            get_misc_options() const;
        misc_options2           get_misc_options2() const;

        bool empty() const;

        bool operator==(const peer_connection_handle& ph) const
        { return m_np == ph.m_np; }

        bool operator!=(const peer_connection_handle& ph) const
        { return m_np != ph.m_np; }

        bool operator<(const peer_connection_handle& ph) const
        { return m_np < ph.m_np; }
    private:
        peer_connection_handle(boost::intrusive_ptr<peer_connection> pc, aux::session_impl* pses);
        aux::session_impl*  m_pses;
        net_identifier      m_np;
    };

}


#endif //__PEER_HANDLE__
