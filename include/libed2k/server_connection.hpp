
#ifndef __LIBED2K_SERVER_CONNECTION__
#define __LIBED2K_SERVER_CONNECTION__

#include <boost/noncopyable.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/asio.hpp>

#include <libtorrent/intrusive_ptr_base.hpp>

#include "libed2k/types.hpp"
#include "libed2k/packet_struct.hpp"
#include "libed2k/error_code.hpp"
#include "libed2k/base_connection.hpp"
#include "libed2k/peer.hpp"
#include "libed2k/session_impl.hpp"

namespace libed2k
{
    namespace aux { class session_impl; }

    const char SC_OFFLINE   = '\x01';
    const char SC_ONLINE    = '\x02';
    const char SC_PROCESS   = '\x04';

    const char SC_TO_ONLINE     = SC_OFFLINE;
    const char SC_TO_OFFLINE    = SC_PROCESS | SC_ONLINE;
    const char SC_TO_SERVER     = SC_ONLINE;


    class server_connection: public libtorrent::intrusive_ptr_base<server_connection>,
                             public boost::noncopyable
    {
        friend class aux::session_impl;
    public:
        server_connection(aux::session_impl& ses);
        ~server_connection();

        /**
          * start working
         */
        void start();

        void stop();

        const tcp::endpoint& serverEndpoint() const;

        bool offline()      const { return m_state == SC_OFFLINE; }
        bool online()       const { return m_state == SC_ONLINE; }
        bool connecting()   const { return m_state == SC_PROCESS; }
        char state()        const { return m_state; }

        boost::uint32_t client_id() const { return m_nClientId; }
        boost::uint32_t tcp_flags() const { return m_nTCPFlags; }
        boost::uint32_t aux_port()  const { return  m_nAuxPort; }


        void post_search_request(search_request& ro);
        void post_search_more_result_request();
        void post_sources_request(const md4_hash& hFile, boost::uint64_t nSize);
        void post_announce(shared_files_list& offer_list);
        void check_keep_alive(int tick_interval_ms);
    private:

        /**
          * close socket and cancel all deadline timers
         */
        void close(const error_code& ec);

        // resolve host name go to connect
        void on_name_lookup(const error_code& error, tcp::resolver::iterator i);
        // connect to host name and go to start
        void on_connection_complete(error_code const& e);
        // file owners were found
        void on_found_peers(const found_file_sources& sources);

        void do_read();

        /**
          * call when socket got packets header
         */
        void handle_read_header(const error_code& error, size_t nSize);

        /**
          * call when socket got packets body and call users call back
         */
        void handle_read_packet(const error_code& error, size_t nSize);

        /**
          * write structures into socket
         */
        template<typename T>
        void do_write(T& t);

        /**
          * order write handler - executed while message order not empty
         */
        void handle_write(const error_code& error, size_t nSize);

        /**
          * deadline timer handler
         */
        void check_deadline();
        bool compatible_state(char c) const;

        int                             m_last_keep_alive_packet;
        char                            m_state;
        boost::uint32_t                 m_nClientId;
        tcp::resolver                   m_name_lookup;
        aux::session_impl&              m_ses;
        boost::uint32_t                 m_nTCPFlags;
        boost::uint32_t                 m_nAuxPort;
        bool                            m_bInitialization;  //!< set true when we wait for connect
        md4_hash                        m_hServer;
        tcp::socket                     m_socket;
        dtimer                          m_deadline;         //!< deadline timer for reading operations

        libed2k_header                  m_in_header;            //!< incoming message header
        socket_buffer                   m_in_container;         //!< buffer for incoming messages
        socket_buffer                   m_in_gzip_container;    //!< special container for compressed data
        tcp::endpoint                   m_target;

        std::deque<std::pair<libed2k_header, std::string> > m_write_order;  //!< outgoing messages order
    };

    template<typename T>
    void server_connection::do_write(T& t)
    {
        bool bWriteInProgress = !m_write_order.empty();

        m_write_order.push_back(std::make_pair(libed2k_header(), std::string()));

        boost::iostreams::back_insert_device<std::string> inserter(m_write_order.back().second);
        boost::iostreams::stream<boost::iostreams::back_insert_device<std::string> > s(inserter);

        // Serialize the data first so we know how large it is.
        archive::ed2k_oarchive oa(s);
        oa << t;
        s.flush();
        m_write_order.back().first.m_size     = m_write_order.back().second.size() + 1;  // packet size without protocol type and packet body size field
        m_write_order.back().first.m_type     = packet_type<T>::value;

        DBG("server_connection::do_write " << packetToString(packet_type<T>::value) << " size: " << m_write_order.back().second.size() + 1);

        if (!bWriteInProgress)
        {
            m_last_keep_alive_packet = 0;   // reset keep alive timeout
            std::vector<boost::asio::const_buffer> buffers;
            buffers.push_back(boost::asio::buffer(&m_write_order.front().first, header_size));
            buffers.push_back(boost::asio::buffer(m_write_order.front().second));

            boost::asio::async_write(m_socket, buffers, boost::bind(&server_connection::handle_write, self(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }
    }
}

#endif
