#include "libed2k/error_code.hpp"

namespace libed2k
{
    const char* libed2k_error_category::name() const
    {
        return "libed2k error";
    }

    std::string libed2k_error_category::message(int ev) const
    {
        static const char* msgs[errors::num_errors] =
        {
            "no error",
            "no memory",
            // serialization errors
            "tag has incorrect type",
            "unexpected output stream error",
            "unexpected input stream error",
            "invalid tag type",
            "blob tag too long",
            "incompatible tag getter",
            "tag list index error",
            "lines syntax error",
            "levels value error",
            "empty or commented line",
            "invalid entry type",               // derived from libtorrent
            "depth exceeded",
            "unexpected eof",
            "expected string",
            "expected value",
            "expected colon",
            "limit exceeded",

            "metadata too large",
            "invalid bencoding",
            "transfer missing piece length",
            "transfer missing name",
            "transfer invalid name",
            "transfer invalid length",
            "transfer file parse failed",
            "transfer missing pieces",
            "transfer invalid hashes",
            "transfer is no dict",
            "transfer info no dict",
            "transfer missing info",
            "unsupported_url_protocol",
            "expected_close_bracket_in_address",
            // protocol errors
            "decode packet error",
            "invalid protocol type",
            "unsupported protocol type",
            "invalid packet size",
            // transport errors
            "session closing",
            "transfer already exists in session",
            "transfer paused",
            "transfer finished",
            "transfer aborted",
            "transfer removed",
            "stopping transfer",
            "timed out",
            "timed out inactivity",
            "connection to itself",
            "duplicate peer id",
            "peer not constructed",
            "banned by IP filter",
            "too many connections",
            "invalid transfer handle",
            "invalid peer connection handle",
            "session pointer is null",
            "destructing transfer",
            "transfer not ready",
            // service errors
            "file size is zero",
            "file was truncated",
            "file not exists or is not regular file",
            "file is too short",
            "file collision",
            "mismatching number of files",
            "mismatching file size",
            "mismatching file timestamp",
            "missing file sizes",
            "missing pieces",
            "not a dictionary",
            "invalid blocks per piece",
            "missing slots",
            "too many slots",
            "invalid slot list",
            "invalid piece index",
            "pieces need reorder",
            "no files in resume data",
            "unclosed quotation mark",
            "operator incorrect place",
            "empty brackets",
            "incorrect brackets count",
            "known file has invalid header on save/load",
            "input string too large",
            "search expression too complex",
            "pending file entry in transform",
            "fast resume parse error",
            "invalid file tag",
            "missing transfer hash",
            "mismatching transfer hash",
            "hashes dont match pieces",
            "failed hash check",
            "invalid escaped string",
            "file parameters making was cancelled"
        };

        if (ev < 0 || ev >= static_cast<int>(sizeof(msgs)/sizeof(msgs[0])))
        {
            return ("unknown error");
        }

        return (msgs[ev]);
    }
}
