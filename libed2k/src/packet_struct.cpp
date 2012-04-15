#include "packet_struct.hpp"

namespace libed2k{

shared_file_entry::shared_file_entry()
{
}

shared_file_entry::shared_file_entry(const md4_hash& hFile, boost::uint32_t nFileId, boost::uint16_t nPort) :
        m_hFile(hFile),
        m_nFileId(nFileId),
        m_nPort(nPort)
{

}

}
