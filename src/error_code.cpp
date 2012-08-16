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
            // protocol errors
            "md4_hash index error",
            "md4_hash convert error",
            "tag has incorrect type",
            "unexpected output stream error",
            "unexpected input stream error",
            "invalid tag type",
            "blob tag too long",
            "incompatible tag getter",
            "tag list index error",
            "decode packet error",
            "invalid protocol type",
            "unsupported protocol type",
            "invalid packet size",
            // transport errors
            "session closing",
            "duplicate transfer",
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
            "too many connections",
            "invalid transfer handle",
            "invalid peer connection handle",
            "session pointer is null",
            "destructing transfer",
            "transfer not ready",
            // service errors
            "file size is zero",
            "file not exists or is not regular file",
            "file is too short",
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
            "invalid escaped string"
        };

        if (ev < 0 || ev >= static_cast<int>(sizeof(msgs)/sizeof(msgs[0])))
        {
            return ("unknown error");
        }

        return (msgs[ev]);
    }
}
