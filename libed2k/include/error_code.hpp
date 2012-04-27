
#ifndef __LIBED2K_ERROR_CODE__
#define __LIBED2K_ERROR_CODE__

#include <exception>
#include <boost/shared_ptr.hpp>
#if defined LIBED2K_WINDOWS || defined LIBED2K_CYGWIN
// asio assumes that the windows error codes are defined already
#include <winsock2.h>
#endif

#include <boost/system/error_code.hpp>

namespace libed2k
{
    namespace errors
    {
        enum error_code_enum
        {
            no_error = 0,
            // protocol errors
            md4_hash_index_error,
            md4_hash_convert_error,
            tag_type_mismatch,
            unexpected_ostream_error,
            unexpected_istream_error,
            invalid_tag_type,
            blob_tag_too_long,
            incompatible_tag_getter,
            tag_list_index_error,
            // transport errors
            session_is_closing,
            duplicate_transfer,
            transfer_finished,
            transfer_aborted,
            stopping_transfer,
            timed_out,
            filesize_is_zero,
            file_unavaliable,

            num_errors
        };
    }
}

namespace boost
{
    namespace system
    {
        template<> struct is_error_code_enum<libed2k::errors::error_code_enum>
        {
            static const bool value = true;
        };

        template<> struct is_error_condition_enum<libed2k::errors::error_code_enum>
        {
            static const bool value = true;
        };
    }
}

namespace libed2k
{
    struct libed2k_error_category : boost::system::error_category
    {
        virtual const char* name() const;
        virtual std::string message(int ev) const;
        virtual boost::system::error_condition default_error_condition(int ev) const
            { return boost::system::error_condition(ev, *this); }
    };

    inline boost::system::error_category& get_libed2k_category()
    {
        static libed2k_error_category libed2k_category;
        return libed2k_category;
    }

    namespace errors
    {
        inline boost::system::error_code make_error_code(error_code_enum e)
        {
            return boost::system::error_code(e, get_libed2k_category());
        }
    }

    using boost::system::error_code;

    struct libed2k_exception: std::exception
    {
        libed2k_exception(error_code const& s): m_error(s) {}

        virtual const char* what() const throw()
        {
            if (!m_msg) m_msg.reset(new std::string(m_error.message()));
            return m_msg->c_str();
        }

        virtual ~libed2k_exception() throw() {}

        error_code error() const
        {
            return m_error;
        }

        private:
            error_code m_error;
            mutable boost::shared_ptr<std::string> m_msg;
    };
}

#endif
