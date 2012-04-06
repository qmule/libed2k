
#include "transfer.hpp"
#include "session_impl.hpp"

using namespace libed2k;

bool transfer::is_paused() const
{
    return m_paused || m_ses.is_paused();
}
