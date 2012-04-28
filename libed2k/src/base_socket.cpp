#include "base_socket.hpp"

namespace libed2k{

    base_socket::base_socket(boost::asio::io_service& io, int nTimeout) :
        m_socket(io),
        m_unhandled_handler(NULL),
        m_handle_error(NULL),
        m_timeout_handler(NULL),
        m_deadline(io),
        m_timeout(nTimeout),
        m_stopped(false),
        m_write_in_progress(false)
    {
        m_deadline.expires_at(boost::posix_time::pos_infin);
    }

    base_socket::~base_socket()
    {
        DBG("base_socket::~base_socket()");
    }

    void base_socket::set_timeout(int nTimeout)
    {
        m_deadline.expires_from_now(boost::posix_time::minutes(200));
    }

    tcp::socket& base_socket::socket()
    {
        return (m_socket);
    }

    void base_socket::start_read_cycle()
    {
        if (m_stopped)
            return;

        check_deadline();
        // set deadline timer
        m_deadline.expires_from_now(boost::posix_time::seconds(m_timeout));

        boost::asio::async_read(m_socket,
                boost::asio::buffer(&m_in_header, header_size),
                boost::bind(&base_socket::handle_read_header,
                        shared_from_this(),
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
    }

    const libed2k_header& base_socket::context() const
    {
        return (m_in_header);
    }

    void base_socket::add_callback(proto_type ptype, socket_handler handler)
    {
        m_callbacks.insert(make_pair(ptype, handler));
    }

    void base_socket::set_unhandled_callback(socket_handler handler)
    {
        m_unhandled_handler = handler;
    }

    void base_socket::set_error_callback(socket_handler handler)
    {
        m_handle_error = handler;
    }

    void base_socket::set_timeout_callback(socket_handler handler)
    {
        m_timeout_handler = handler;
    }

    void base_socket::close()
    {
        DBG("base_socket::close()");
        m_stopped = true;
        boost::system::error_code ignored_ec;
        m_socket.close(ignored_ec);
        m_deadline.cancel();
    }

    void base_socket::handle_write(const error_code& error, size_t nSize)
    {
        if (m_stopped)
            return;

        if (!error) {
            m_send_buffer.pop_front(nSize);
            m_write_in_progress = false;
            setup_send();
        }
        else
            handle_error(error);
    }

    void base_socket::handle_error(const error_code& error)
    {
        ERR("base socket error: " << error.message());

        m_socket.close();

        if (m_handle_error)
        {
            m_handle_error(error);
        }
    }

    void base_socket::handle_read_header(const error_code& error, size_t nSize)
    {
        if (m_stopped)
            return;

        if (!error)
        {
            // we must download body in any case
            // increase internal buffer size if need
            if (m_in_container.size() < m_in_header.m_size - 1)
            {
                m_in_container.resize(m_in_header.m_size - 1);
            }

            boost::asio::async_read(m_socket, boost::asio::buffer(&m_in_container[0], m_in_header.m_size - 1),
                    boost::bind(&base_socket::handle_read_packet, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        }
        else
        {
            handle_error(error);
        }
    }

    void base_socket::handle_read_packet(const error_code& error, size_t nSize)
    {
        if (m_stopped)
            return;

        if (!error)
        {
            //!< search appropriate dispatcher
            callback_map::iterator itr = m_callbacks.find(m_in_header.m_type);

            if (itr != m_callbacks.end())
            {
                DBG("call normal handler");
                itr->second(error);
            }
            else
            {
                if (m_unhandled_handler)
                {
                    DBG("call unhandled ");
                    m_unhandled_handler(error);
                }
                else
                {
                    DBG("unhandled handler is null");
                }
            }

            start_read_cycle();

        }
        else
        {
            handle_error(error);
        }
    }

    void base_socket::check_deadline()
    {
        if (m_stopped)
            return;

        // Check whether the deadline has passed. We compare the deadline against
        // the current time since a new asynchronous operation may have moved the
        // deadline before this actor had a chance to run.

        if (m_deadline.expires_at() <= dtimer::traits_type::now())
        {
            DBG("base_socket::check_deadline(): deadline timer expired");

            // The deadline has passed. The socket is closed so that any outstanding
            // asynchronous operations are cancelled.
            m_socket.close();
            // There is no longer an active deadline. The expiry is set to positive
            // infinity so that the actor takes no action until a new deadline is set.
            m_deadline.expires_at(boost::posix_time::pos_infin);
            boost::system::error_code ignored_ec;

            if (m_timeout_handler)
            {
                m_timeout_handler(ignored_ec);
            }
        }

        // Put the actor back to sleep.
        m_deadline.async_wait(boost::bind(&base_socket::check_deadline, shared_from_this()));
    }

    void base_socket::copy_send_buffer(char const* buf, int size)
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

        std::pair<char*, int> buffer = allocate_buffer(size);
        if (buffer.first == 0)
        {
            handle_error(errors::no_memory);
            return;
        }

        std::memcpy(buffer.first, buf, size);
        m_send_buffer.append_buffer(
            buffer.first, buffer.second, size,
            boost::bind(
                &base_socket::free_buffer, shared_from_this(), _1, buffer.second));
    }

    void base_socket::setup_send()
    {
        // send the actual buffer
        if (!m_write_in_progress && !m_send_buffer.empty())
        {
            // check quota here
            int amount_to_send = m_send_buffer.size();

            // set deadline timer
            m_deadline.expires_from_now(boost::posix_time::seconds(m_timeout));

            const std::list<boost::asio::const_buffer>& buffers =
                m_send_buffer.build_iovec(amount_to_send);
            // TODO: possible bad reference counting here
            boost::asio::async_write(
                m_socket, buffers,
                boost::bind(&base_socket::handle_write, shared_from_this(), _1, _2));
            m_write_in_progress = true;
        }
    }

    std::pair<char*, int> base_socket::allocate_buffer(int size)
    {
        return std::make_pair(new char[size], size);
    }

    void base_socket::free_buffer(char* buf, int size)
    {
        delete[] buf;
    }
}
