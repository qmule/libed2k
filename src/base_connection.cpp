#include "libed2k/base_connection.hpp"
#include "libed2k/session.hpp"
#include "libed2k/session_impl.hpp"

namespace libed2k
{
    base_connection::base_connection(aux::session_impl& ses):
        m_ses(ses), m_socket(new tcp::socket(ses.m_io_service)),
        m_deadline(ses.m_io_service)
    {
        reset();
    }

    base_connection::base_connection(
        aux::session_impl& ses, boost::shared_ptr<tcp::socket> s, 
        const tcp::endpoint& remote):
        m_ses(ses), m_socket(s), m_deadline(ses.m_io_service), m_remote(remote)
    {
        reset();
    }

    base_connection::~base_connection()
    {
    }

    void base_connection::reset()
    {
        m_deadline.expires_at(max_time());
        m_channel_state[upload_channel] = peer_info::bw_idle;
        m_channel_state[download_channel] = peer_info::bw_idle;
        m_disconnecting = false;
    }

    void base_connection::disconnect(const error_code& ec, int error)
    {
        DBG("close connection {remote: " << m_remote << ", msg: "<< ec.message() << "}");
        m_disconnecting = true;
        m_socket->close();
        m_deadline.cancel();
    }

    void base_connection::do_read()
    {
        if (is_closed()) return;
        if (m_channel_state[download_channel] & (peer_info::bw_network | peer_info::bw_limit)) return;

        m_deadline.expires_from_now(seconds(m_ses.settings().peer_timeout));
        boost::asio::async_read(
            *m_socket, boost::asio::buffer(&m_in_header, header_size),
            boost::bind(&base_connection::on_read_header, self(), _1, _2));
        m_channel_state[download_channel] |= peer_info::bw_network;
    }

    void base_connection::do_write(int quota)
    {
        if (is_closed()) return;
        if (m_channel_state[upload_channel] & (peer_info::bw_network | peer_info::bw_limit)) return;

        int amount_to_send = std::min<int>(m_send_buffer.size(), quota);
        if (amount_to_send == 0) return;

        // set deadline timer
        m_deadline.expires_from_now(seconds(m_ses.settings().peer_timeout));

        const std::list<boost::asio::const_buffer>& buffers =
            m_send_buffer.build_iovec(amount_to_send);
        boost::asio::async_write(*m_socket, buffers, make_write_handler(
                                     boost::bind(&base_connection::on_write, self(), _1, _2)));
        m_channel_state[upload_channel] |= peer_info::bw_network;
    }

    void base_connection::write_message(const message& msg) {
        copy_send_buffer((char*)(&msg.header), header_size);
        copy_send_buffer(msg.body.c_str(), msg.body.size());

        do_write();
    }

    void base_connection::copy_send_buffer(char const* buf, int size)
    {
        int free_space = m_send_buffer.space_in_last_buffer();
        if (free_space > size) free_space = size;
        if (free_space > 0)
        {
            m_send_buffer.append(buf, free_space);
            size -= free_space;
            buf += free_space;
        }
        if (size <= 0) return;

        std::pair<char*, int> buffer = m_ses.allocate_send_buffer(size);
        if (buffer.first == 0)
        {
            disconnect(errors::no_memory);
            return;
        }

        std::memcpy(buffer.first, buf, size);
        m_send_buffer.append_buffer(
            buffer.first, buffer.second, size,
            boost::bind(&aux::session_impl::free_send_buffer,
                        boost::ref(m_ses), _1, buffer.second));
    }

    void base_connection::on_timeout(const error_code& e)
    {
    }

    void base_connection::on_read_header(const error_code& error, size_t nSize)
    {
        boost::mutex::scoped_lock l(m_ses.m_mutex);

        // keep ourselves alive in until this function exits in
        // case we disconnect
        boost::intrusive_ptr<base_connection> me(self());

        m_statistics.received_bytes(0, nSize);
        if (is_closed()) return;

        error_code ec = error;

        if (!ec)
        {
            ec = m_in_header.check_packet();
        }

        if (!ec)
        {
            size_t size = m_in_header.service_size();

            switch(m_in_header.m_protocol)
            {
                case OP_EDONKEYPROT:
                case OP_EMULEPROT:
                {
                    m_in_container.resize(size);

                    if (size == 0)
                    {
                        // all data was read - execute callback
                        m_ses.m_io_service.post(
                            boost::bind(&base_connection::on_read_packet,
                                        self(), boost::system::error_code(), 0));
                    }
                    else
                    {
                        boost::asio::async_read(
                            *m_socket, boost::asio::buffer(&m_in_container[0], size),
                            boost::bind(&base_connection::on_read_packet, self(), _1, _2));
                    }
                    break;
                }
                case OP_PACKEDPROT:
                {
                    m_in_gzip_container.resize(size);
                    boost::asio::async_read(*m_socket, boost::asio::buffer(&m_in_gzip_container[0], size),
                            boost::bind(&base_connection::on_read_packet, self(), _1, _2));
                    break;
                }
                default:
                    assert(false);
                    break;
            }
        }
        else
        {
            disconnect(ec);
        }

    }

    void base_connection::on_read_packet(const error_code& error, size_t nSize)
    {
        boost::mutex::scoped_lock l(m_ses.m_mutex);

        // keep ourselves alive in until this function exits in
        // case we disconnect
        boost::intrusive_ptr<base_connection> me(self());

        m_statistics.received_bytes(0, nSize);
        if (is_closed()) return;

        if (!error)
        {
            m_channel_state[download_channel] &= ~peer_info::bw_network;

            //!< search appropriate dispatcher
            handler_map::iterator itr = m_handlers.find(
                std::make_pair(m_in_header.m_type, m_in_header.m_protocol));

            if (itr != m_handlers.end())
            {
                itr->second(error);
            }
            else
            {
                DBG("ignore unhandled packet: " << std::hex << int(m_in_header.m_type) << " <<< " << m_remote);
            }

            m_in_gzip_container.clear();
            m_in_container.clear();

            // don't read data as header
            if (m_in_header.m_type != OP_SENDINGPART && m_in_header.m_type != OP_SENDINGPART_I64)
                do_read();
        }
        else
        {
            disconnect(error);
        }
    }

    void base_connection::on_write(const error_code& error, size_t nSize)
    {
        boost::mutex::scoped_lock l(m_ses.m_mutex);

        // keep ourselves alive in until this function exits in
        // case we disconnect
        boost::intrusive_ptr<base_connection> me(self());

        LIBED2K_ASSERT(m_channel_state[upload_channel] & peer_info::bw_network);

        m_send_buffer.pop_front(nSize);

        m_channel_state[upload_channel] &= ~peer_info::bw_network;

        if (error) {
            disconnect(error);
            return;
        }
        if (is_closed()) return;

        on_sent(error, nSize);

        do_write();
    }

    void base_connection::check_deadline()
    {
        if (is_closed()) return;

        // Check whether the deadline has passed. We compare the deadline against
        // the current time since a new asynchronous operation may have moved the
        // deadline before this actor had a chance to run.
        if (m_deadline.expires_at() <= deadline_timer::traits_type::now())
        {
            DBG("base_connection::check_deadline(): deadline timer expired");

            // The deadline has passed. The socket is closed so that any outstanding
            // asynchronous operations are cancelled.
            disconnect(errors::timed_out);
            // There is no longer an active deadline. The expiry is set to positive
            // infinity so that the actor takes no action until a new deadline is set.
            m_deadline.expires_at(max_time());
            boost::system::error_code ignored_ec;

            on_timeout(ignored_ec);
        }

        // Put the actor back to sleep.
        m_deadline.async_wait(boost::bind(&base_connection::check_deadline, self()));
    }

    void base_connection::add_handler(std::pair<proto_type, proto_type> ptype, packet_handler handler)
    {
        m_handlers.insert(make_pair(ptype, handler));
    }

}
