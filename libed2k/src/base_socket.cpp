#include "base_socket.hpp"

namespace libed2k{

    base_socket::base_socket(boost::asio::io_service& io, int nTimeout) :
        m_socket(io),
        m_unhandled_handler(NULL),
        m_handle_error(NULL),
        m_timeout_handler(NULL),
        m_deadline(io),
        m_timeout(nTimeout),
        m_stopped(false)
    {
        m_deadline.expires_at(boost::posix_time::pos_infin);
        check_deadline();
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

    void base_socket::async_read()
    {
        if (m_stopped)
            return;

        // set deadline timer
        m_deadline.expires_from_now(boost::posix_time::seconds(m_timeout));

        boost::asio::async_read(m_socket,
                boost::asio::buffer(&m_in_header, header_size),
                boost::bind(&base_socket::handle_read_header,
                        this,
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
        DBG("base socket close");
        m_stopped = true;
        boost::system::error_code ignored_ec;
        m_socket.close(ignored_ec);
        m_deadline.cancel();
    }

    void base_socket::handle_write(const error_code& error, size_t nSize)
    {
        if (m_stopped)
            return;

        if (!error)
        {
            m_write_order.pop_front();

            if (!m_write_order.empty())
            {
                // set deadline timer
                m_deadline.expires_from_now(boost::posix_time::seconds(m_timeout));

                std::vector<boost::asio::const_buffer> buffers;
                buffers.push_back(boost::asio::buffer(&m_write_order.front().first, header_size));
                buffers.push_back(boost::asio::buffer(m_write_order.front().second));
                boost::asio::async_write(m_socket, buffers, boost::bind(&base_socket::handle_write, this,
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::bytes_transferred));
            }
        }
        else
        {
            handle_error(error);
        }
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
                    boost::bind(&base_socket::handle_read_packet, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
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
                std::string strData = "ddfd";
                LDBG_ << "call normal handler";
                LDBG_ << strData;
                itr->second(error);
            }
            else
            {
                if (m_unhandled_handler)
                {
                    LDBG_ << "call unhandled ";
                    m_unhandled_handler(error);
                }
                else
                {
                    LDBG_ << "unhandled handler is null";
                }
            }

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
        m_deadline.async_wait(boost::bind(&base_socket::check_deadline, this));
    }

}
