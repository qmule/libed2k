
#ifndef __LIBED2K_SERVER_CONNECTION__
#define __LIBED2K_SERVER_CONNECTION__

#include <boost/noncopyable.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/asio.hpp>

#include <libtorrent/intrusive_ptr_base.hpp>

#include "types.hpp"
#include "packet_struct.hpp"
#include "error_code.hpp"
#include "base_connection.hpp"
#include "peer.hpp"
#include "session_impl.hpp"

namespace libed2k
{
    namespace aux { class session_impl; }

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

        /**
          * close socket and cancel all deadline timers
         */
        void close(const error_code& ec);

        /**
          * connection stopped when his socket is not opened
         */
        bool is_stopped() const;

        /**
          * return true when connection in initialization process
         */
        bool initializing() const;

        const tcp::endpoint& getServerEndpoint() const;

        void post_search_request(request_order& ro);
        void post_sources_request(const md4_hash& hFile, boost::uint64_t nSize);
        void post_announce(offer_files_list& offer_list);
    private:

        // resolve host name go to connect
        void on_name_lookup(const error_code& error, tcp::resolver::iterator i);
        // resolve udp host name go to connect
        void on_udp_name_lookup(const error_code& error, udp::resolver::iterator i);
        // connect to host name and go to start
        void on_connection_complete(error_code const& e);
        // file owners were found
        void on_found_peers(const found_file_sources& sources);

        void write_server_keep_alive();

        void do_read();
        void do_read_udp();

        /**
          * call when socket got packets header
         */
        void handle_read_header(const error_code& error, size_t nSize);

        /**
          * call when socket got packets body and call users call back
         */
        void handle_read_packet(const error_code& error, size_t nSize);

        /**
          * call when socket got packets header
         */
        void handle_read_header_udp(const error_code& error, size_t nSize);

        /**
          * call when socket got packets body and call users call back
         */
        void handle_read_packet_udp(const error_code& error, size_t nSize);

        /**
          * write structures into socket
         */
        template<typename T>
        void do_write(T& t);

        template<typename T>
        void do_write_udp(T& t);


        /**
          * order write handler - executed while message order not empty
         */
        void handle_write(const error_code& error, size_t nSize);
        void handle_write_udp(const error_code& error, size_t nSize);

        /**
          * deadline timer handler
         */
        void check_deadline();

        boost::uint32_t                 m_nClientId;
        tcp::resolver                   m_name_lookup;
        dtimer                          m_keep_alive;       //!< timer for ping server
        aux::session_impl&              m_ses;
        boost::uint32_t                 m_nTCPFlags;
        boost::uint32_t                 m_nAuxPort;
        bool                            m_bInitialization;  //!< set true when we wait for connect
        tcp::socket                     m_socket;
        dtimer                          m_deadline;         //!< deadline timer for reading operations

        udp::socket                     m_udp_socket;
        udp::resolver                   m_udp_name_lookup;
        libed2k_header                  m_in_udp_header;        //!< incoming message header
        socket_buffer                   m_in_udp_container;     //!< buffer for incoming messages
        udp::endpoint                   m_udp_target;

        libed2k_header                  m_in_header;            //!< incoming message header
        socket_buffer                   m_in_container;         //!< buffer for incoming messages
        socket_buffer                   m_in_gzip_container;    //!< special container for compressed data
        tcp::endpoint                   m_target;

        std::deque<std::pair<libed2k_header, std::string> > m_write_order;  //!< outgoing messages order
        std::deque<std::pair<libed2k_header, std::string> > m_udp_order;    //!< outgoing messages order for udp
    };

    template<typename T>
    void server_connection::do_write(T& t)
    {
        if (is_stopped())
        {
            return;
        }

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
            std::vector<boost::asio::const_buffer> buffers;
            buffers.push_back(boost::asio::buffer(&m_write_order.front().first, header_size));
            buffers.push_back(boost::asio::buffer(m_write_order.front().second));

            // set deadline timer
            m_deadline.expires_from_now(boost::posix_time::seconds(
                                            m_ses.settings().server_timeout));

            boost::asio::async_write(m_socket, buffers, boost::bind(&server_connection::handle_write, self(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }
    }

    template<typename T>
    void server_connection::do_write_udp(T& t)
    {
        if (!m_udp_socket.is_open())
        {
            return;
        }

        bool bWriteInProgress = !m_udp_order.empty();

        m_udp_order.push_back(std::make_pair(libed2k_header(), std::string()));

        boost::iostreams::back_insert_device<std::string> inserter(m_udp_order.back().second);
        boost::iostreams::stream<boost::iostreams::back_insert_device<std::string> > s(inserter);

        // Serialize the data first so we know how large it is.
        archive::ed2k_oarchive oa(s);
        oa << t;
        s.flush();
        m_udp_order.back().first.m_size     = m_udp_order.back().second.size() + 1;  // packet size without protocol type and packet body size field
        m_udp_order.back().first.m_type     = packet_type<T>::value;

        DBG("server_connection::do_write_udp " << packetToString(packet_type<T>::value) << " size: " << m_udp_order.back().second.size() + 1);

        if (!bWriteInProgress)
        {
            std::vector<boost::asio::const_buffer> buffers;
            buffers.push_back(boost::asio::buffer(&m_udp_order.front().first, header_size));
            buffers.push_back(boost::asio::buffer(m_udp_order.front().second));

            m_udp_socket.async_send_to(buffers, m_udp_target, boost::bind(&server_connection::handle_write_udp, self(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }
    }

}

#endif
