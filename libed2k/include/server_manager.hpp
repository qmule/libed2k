
#ifndef __LIBED2K_SERVER_MANAGER__
#define __LIBED2K_SERVER_MANAGER__

namespace libed2k{

	namespace aux { struct session_impl; }

    class server_manager {
    public:

        server_manager(aux::session_impl& ses):
            m_ses(ses),
            m_abort(false) {}

    private:

        aux::session_impl& m_ses;
        bool m_abort;
    };

}

#endif
