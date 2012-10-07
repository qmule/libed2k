#include <algorithm>
#include <locale.h>

#ifndef WIN32
#include <iconv.h>
#endif

#include <libed2k/utf8.hpp>
#include <libed2k/util.hpp>
#include <libed2k/file.hpp>

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
        size_type end = std::min<size_type>(begin + BLOCK_SIZE, std::min(align_size, size));
        assert(begin < end);
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


const char HEX2DEC[256] = 
{
    /*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
    /* 0 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 1 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 2 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,
    
    /* 4 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 5 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 6 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 7 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    
    /* 8 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 9 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* A */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* B */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    
    /* C */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* D */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* E */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* F */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};

    std::string url_decode(const std::string& s)
    {      
        std::string strRes;
        std::string::size_type index = 0;

        if (s.size() > 2)
        {
            std::string::size_type last = s.size() - 2;

            while(index < last)
            {
                if (s[index] == '%')
                {
                    char dec1, dec2;
                    if (-1 != (dec1 = HEX2DEC[s[index+1]]) && -1 != (dec2 = HEX2DEC[s[index+2]]))
                    {
                        char c = (dec1 << 4) + dec2;
                        strRes += c;
                        index+= 3;
                        continue;
                    }
                }            

                strRes += s[index];
                ++index;
            }
        }

        while(index < s.size())
        {
            strRes += s[index];
            ++index;
        }

        return (strRes);
    }
}
