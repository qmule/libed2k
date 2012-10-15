#include <algorithm>
#include <locale.h>

#ifndef WIN32
#include <iconv.h>
#endif

#include "libed2k/size_type.hpp"
#include "libed2k/utf8.hpp"
#include "libed2k/util.hpp"
#include "libed2k/file.hpp"

namespace libed2k 
{
    std::string bom_filter(const std::string& s)
    {
        if (CHECK_BOM(s.size(), s))
        {
            return s.substr(3);
        }

        return s;
    }

    std::string bitfield2string(const bitfield& bits)
    {
        std::stringstream str;
        for(size_t i = 0; i < bits.size(); ++i) str << bits[i];
        return str.str();
    }

    std::pair<size_type, size_type> block_range(int piece, int block, size_type size)
    {
        size_type begin = piece * PIECE_SIZE + block * BLOCK_SIZE;
        size_type align_size = (piece + 1) * PIECE_SIZE;
        size_type end = std::min<size_type>(begin + BLOCK_SIZE, std::min<size_type>(align_size, size));
        LIBED2K_ASSERT(begin < end);
        return std::make_pair(begin, end);
    }

    duration_timer::duration_timer(const time_duration& duration, const ptime& last_tick):
        m_duration(duration), m_last_tick(last_tick), m_tick_interval()
    {
    }

    bool duration_timer::expires()
    {
        ptime now = time_now_hires();
        m_tick_interval = now - m_last_tick;
        if (m_tick_interval < m_duration) return false;
        m_last_tick = now;
        return true;
    }

#if 0
    int inflate_gzip(const socket_buffer& vSrc, socket_buffer& vDst, int nMaxSize)
    {
        // start off with one kilobyte and grow
        // if needed
        vDst.resize(1024);

        // initialize the zlib-stream
        z_stream str;

        // subtract 8 from the end of the buffer since that's CRC32 and input size
        // and those belong to the gzip file
        str.zalloc      = Z_NULL;
        str.zfree       = Z_NULL;
        str.opaque      = Z_NULL;
        str.avail_in    = 0;
        str.next_in     = Z_NULL;
        int ret = inflateInit(&str);

        if (ret != Z_OK)
            return ret;

        str.avail_in    = static_cast<int>(vSrc.size());
        str.next_in     = reinterpret_cast<Bytef*>(const_cast<char*>(&vSrc[0]));

        str.next_out    = reinterpret_cast<Bytef*>(&vDst[0]);
        str.avail_out   = static_cast<int>(vDst.size());
        str.zalloc      = Z_NULL;
        str.zfree       = Z_NULL;
        str.opaque      = 0;

        // inflate and grow inflate_buffer as needed
        ret = inflate(&str, Z_SYNC_FLUSH);

        while (ret == Z_OK)
        {
            if (str.avail_out == 0)
            {
                if (vDst.size() >= (unsigned)nMaxSize)
                {
                    inflateEnd(&str);
                    return Z_MEM_ERROR;
                }

                int nNewSize = (int)vDst.size() * 2;
                if (nNewSize > nMaxSize) nNewSize = nMaxSize;

                int nOldSize = (int)vDst.size();

                vDst.resize(nNewSize);
                str.next_out = reinterpret_cast<Bytef*>(&vDst[nOldSize]);
                str.avail_out = nNewSize - nOldSize;
            }

            ret = inflate(&str, Z_SYNC_FLUSH);
        }

        vDst.resize(vDst.size() - str.avail_out);
        inflateEnd(&str);
        return (ret);
    }
#endif
}
