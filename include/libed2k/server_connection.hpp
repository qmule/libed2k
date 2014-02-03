
#ifndef __LIBED2K_SERVER_CONNECTION__
#define __LIBED2K_SERVER_CONNECTION__

#include "libed2k/base_connection.hpp"
#include "libed2k/peer.hpp"
#include "libed2k/session_impl.hpp"

namespace libed2k
{
    namespace aux { class session_impl; }

    struct server_connection_parameters{
        std::string     hostname;
        std::string     port;
        time_duration   operations_timeout;
        time_duration   keep_alive_timeout;
        time_duration   reconnect_timeout;
        server_connection_parameters();
    };

#define CHECK_ABORTED() if (current_operation != scs_handshake && current_operation != scs_start) { return; }

    class server_connection: public intrusive_ptr_base<server_connection>,
                             public boost::noncopyable
    {
        friend class aux::session_impl;
    public:

        enum sc_state{
            scs_stop,
            scs_resolve,
            scs_connection,
            scs_handshake,
            scs_start
        };

        server_connection(aux::session_impl& ses);
        ~server_connection();

        void start(const server_connection_parameters& p);
        void stop(const error_code& ec);

        boost::uint32_t client_id() const { return m_client_id; }
        boost::uint32_t tcp_flags() const { return m_tcp_flags; }
        boost::uint32_t aux_port()  const { return  m_aux_port; }


        void post_search_request(search_request& ro);
        void post_search_more_result_request();
        void post_sources_request(const md4_hash& hFile, boost::uint64_t nSize);
        void post_announce(shared_files_list& offer_list);
        void post_callback_request(client_id_type);
        void second_tick(int tick_interval_ms);
    private:

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

        client_id_type                  m_client_id;
        tcp::resolver                   m_name_lookup;
        aux::session_impl&              m_ses;
        boost::uint32_t                 m_tcp_flags;
        boost::uint32_t                 m_aux_port;
        md4_hash                        m_hServer;
        tcp::socket                     m_socket;

        libed2k_header                  m_in_header;            //!< incoming message header
        socket_buffer                   m_in_container;         //!< buffer for incoming messages
        socket_buffer                   m_in_gzip_container;    //!< special container for compressed data
        tcp::endpoint                   m_target;

        std::deque<std::pair<libed2k_header, std::string> > m_write_order;  //!< outgoing messages order
        sc_state                        current_operation;
        ptime                           last_action_time;
        server_connection_parameters    params;
    };

    template<typename T>
    void server_connection::do_write(T& t)
    {
        // temporary assert
        LIBED2K_ASSERT(current_operation == scs_handshake || current_operation == scs_start);

        CHECK_ABORTED()

        last_action_time = time_now();
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

            boost::asio::async_write(m_socket, buffers, boost::bind(&server_connection::handle_write, self(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }
    }
}

#endif
