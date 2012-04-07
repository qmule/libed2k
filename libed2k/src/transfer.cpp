
#include "transfer.hpp"
#include "session_impl.hpp"

using namespace libed2k;

transfer::transfer(aux::session_impl& ses, tcp::endpoint const& net_interface, 
                   int block_size, int seq, add_transfer_params const& p):
    m_ses(ses), m_paused(false), m_sequential_download(false), m_sequence_number(seq)
{
    // TODO: init here
}

transfer::~transfer()
{
}

void transfer::start()
{
    // TODO: transfer start here
}

bool transfer::is_paused() const
{
    return m_paused || m_ses.is_paused();
}
