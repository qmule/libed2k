// ed2k session

#ifndef __LIBED2K_SESSION__
#define __LIBED2K_SESSION__

#include <string>
#include <boost/shared_ptr.hpp>

#include "fingerprint.hpp"

namespace libed2k
{
    namespace aux {
        class session_impl;
    }

    // Once it's created, the session object will spawn the main thread
    // that will do all the work. The main thread will be idle as long 
    // it doesn't have any transfers to participate in.
    // TODO: should inherit the session_base interfase in future
    class session
    {
    public:
        session(int listen_port, const char* listen_interface,
                const fingerprint& id, const std::string& logpath = ".")
        {
            init(listen_port, listen_interface, id, logpath);
        }

    private:
        void init(int listen_port, const char* listen_interface,
                  const fingerprint& id, const std::string& logpath);

		// data shared between the main thread
		// and the working thread
        boost::shared_ptr<aux::session_impl> m_impl;
    };
}

#endif
