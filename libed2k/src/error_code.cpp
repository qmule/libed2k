#include "error_code.hpp"

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
            // protocol errors
            "tag has incorrect type",
            "unexpected output stream error",
            "unexpected input stream error",
            "invalid tag type",
            "blob tag too long",
            "incompatible tag getter",
            "save undefined tag error",
            "tag list index error",
            // transport errors
            "session is closing",
            "duplicate transfer",
            "transfer finished"
        };

        if (ev < 0 || ev >= static_cast<int>(sizeof(msgs)/sizeof(msgs[0])))
        {
            return ("unknown error");
        }

        return (msgs[ev]);
    }
}
