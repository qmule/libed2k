// ed2k transfer handle

#ifndef __LIBED2K_TRANSFER_HANDLE__
#define __LIBED2K_TRANSFER_HANDLE__

namespace libed2k
{
    class transfer;
    namespace aux {
        class session_impl;
    }

    // We will usually have to store our transfer handles somewhere, 
    // since it's the object through which we retrieve information 
    // about the transfer and aborts the transfer.
    struct transfer_handle
    {
        friend class aux::session_impl;
        friend class transfer;

        transfer_handle() {}

    private:

        transfer_handle(const boost::weak_ptr<transfer>& t):
            m_transfer(t)
        {}

        boost::weak_ptr<transfer> m_transfer;
    };

}

#endif
