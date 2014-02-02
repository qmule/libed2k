
#ifndef __LIBED2K_ERROR_CODE__
#define __LIBED2K_ERROR_CODE__

#include <exception>
#include <boost/shared_ptr.hpp>
#if defined LIBED2K_WINDOWS || defined LIBED2K_CYGWIN
// asio assumes that the windows error codes are defined already
#include <winsock2.h>
#endif

#include <boost/system/error_code.hpp>

#ifndef BOOST_SYSTEM_NOEXCEPT
#define BOOST_SYSTEM_NOEXCEPT throw()
#endif

namespace libed2k
{
    namespace errors
    {
        enum error_code_enum
        {
            no_error = 0,
            no_memory,
            // serialization errors
            tag_type_mismatch,
            unexpected_ostream_error,
            unexpected_istream_error,
            invalid_tag_type,
            blob_tag_too_long,
            incompatible_tag_getter,
            tag_list_index_error,
            lines_syntax_error,
            levels_value_error,
            empty_or_commented_line,
            invalid_entry_type,         // derived from libtorrent
            depth_exceeded,
            unexpected_eof,
            expected_string,
            expected_value,
            expected_colon,
            limit_exceeded,

            metadata_too_large,
            invalid_bencoding,
            transfer_missing_piece_length,
            transfer_missing_name,
            transfer_invalid_name,
            transfer_invalid_length,
            transfer_file_parse_failed,
            transfer_missing_pieces,
            transfer_invalid_hashes,
            transfer_is_no_dict,
            transfer_info_no_dict,
            transfer_missing_info,
            unsupported_url_protocol,
            expected_close_bracket_in_address,
            // protocol errors
            decode_packet_error,
            invalid_protocol_type,
            unsupported_protocol_type,
            invalid_packet_size,
            // transport errors
            session_closing,
            duplicate_transfer,
            transfer_paused,
            transfer_finished,
            transfer_aborted,
            transfer_removed,
            stopping_transfer,
            no_router,
            timed_out,
            timed_out_inactivity,
            self_connection,
            duplicate_peer_id,
            peer_not_constructed,
            banned_by_ip_filter,
            too_many_connections,
            invalid_transfer_handle,
            invalid_peer_connection_handle,
            session_pointer_is_null,
            destructing_transfer,
            transfer_not_ready,
            // HTTP errors
            http_parse_error,
            http_missing_location,
            http_failed_decompress,
            // i2p errors
            no_i2p_router,
            // service errors
            filesize_is_zero,
            file_was_truncated,
            file_unavaliable,
            file_too_short,
            file_collision,
            mismatching_number_of_files,
            mismatching_file_size,
            mismatching_file_timestamp,
            missing_file_sizes,
            missing_pieces,
            not_a_dictionary,
            invalid_blocks_per_piece,
            missing_slots,
            too_many_slots,
            invalid_slot_list,
            invalid_piece_index,
            pieces_need_reorder,
            no_files_in_resume_data,
            unclosed_quotation_mark,
            operator_incorrect_place,
            empty_brackets,
            incorrect_brackets_count,
            met_file_invalid_header_byte,
            input_string_too_large,
            search_expression_too_complex,
            pending_file_entry_in_transform,
            fast_resume_parse_error,
            invalid_file_tag,
            missing_transfer_hash,
            mismatching_transfer_hash,
            hashes_dont_match_pieces,
            failed_hash_check,
            invalid_escaped_string,
            file_params_making_was_cancelled,
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

    using boost::system::error_code;
    inline boost::system::error_category const& get_system_category()
    { return boost::system::get_system_category(); }

    inline boost::system::error_category const& get_posix_category()

#if BOOST_VERSION < 103600
    { return boost::system::get_posix_category(); }
#else
    { return boost::system::get_generic_category(); }
#endif // BOOST_VERSION < 103600


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
