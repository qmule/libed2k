
// ed2k peer connection

#ifndef __LIBED2K_PEER_CONNECTION__
#define __LIBED2K_PEER_CONNECTION__

#include "libtorrent/intrusive_ptr_base.hpp"

namespace libed2k
{

    class peer_connection : public libtorrent::intrusive_ptr_base<peer_connection>,
                            public boost::noncopyable
    {
    public:
    };

}

#endif
