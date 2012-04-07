#include "error_code.hpp"

namespace libed2k
{
    const char* libed2k_error_category::name() const
    {
        return "libed2k error";
    }

    std::string libed2k_error_category::message(int ev) const
    {
        static const char* msgs[] =
        {
            "no error",
            "tag has incorrect type"
        };

        if (ev < 0 || ev >= static_cast<int>(sizeof(msgs)/sizeof(msgs[0])))
        {
            return ("unknown error");
        }

        return (msgs[ev]);
    }
}
