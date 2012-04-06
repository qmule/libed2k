// ed2k transfer

#ifndef __LIBED2K_TRANSFER__
#define __LIBED2K_TRANSFER__

namespace libed2k
{
    namespace aux{
        class session_impl;
    }

	// a transfer is a class that holds information
	// for a specific download. It updates itself against
	// the tracker
    class transfer
    {
    public:
        bool is_paused() const;
    private:

        // a back reference to the session
        // this transfer belongs to.
        aux::session_impl& m_ses;

        bool m_paused;
    };
}

#endif
