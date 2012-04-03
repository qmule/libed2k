// ed2k session network thread

#ifndef __LIBED2K_SESSION_IMPL__
#define __LIBED2K_SESSION_IMPL__

#include <string>
#include "fingerprint.hpp"

namespace libed2k {
    namespace aux {
     
        class session_impl
        {
        public:
            session_impl(int listen_port, const char* listen_interface,
                         const fingerprint& id, const std::string& logpath);
        };
    }
}

#endif
