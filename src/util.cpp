#include <algorithm>
#include <locale.h>
#include <cctype>

#ifndef WIN32
#include <iconv.h>
#endif

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include "libed2k/size_type.hpp"
#include "libed2k/utf8.hpp"
#include "libed2k/util.hpp"
#include "libed2k/file.hpp"
#include "libed2k/bitfield.hpp"
#include "libed2k/log.hpp"

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

    bool duration_timer::expired(const ptime& now)
    {
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
    
    // Only alphanum is safe.
    const char SAFE[256] =
    {
        /*      0 1 2 3  4 5 6 7  8 9 A B  C D E F */
        /* 0 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* 1 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* 2 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* 3 */ 1,1,1,1, 1,1,1,1, 1,1,0,0, 0,0,0,0,

        /* 4 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
        /* 5 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,0,0,
        /* 6 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
        /* 7 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,0,0,

        /* 8 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* 9 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* A */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* B */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

        /* C */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* D */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* E */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* F */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
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

    std::string url_encode(const std::string & s)
    {
        const char DEC2HEX[16 + 1] = "0123456789ABCDEF";
        const unsigned char* pSrc = (const unsigned char *)s.c_str();
        const int SRC_LEN = s.length();
        unsigned char * const pStart = new unsigned char[SRC_LEN * 3];
        unsigned char * pEnd = pStart;
        const unsigned char * const SRC_END = pSrc + SRC_LEN;

        for (; pSrc < SRC_END; ++pSrc)
        {
            if (SAFE[*pSrc])
                *pEnd++ = *pSrc;
            else
            {
                // escape this char
                *pEnd++ = '%';
                *pEnd++ = DEC2HEX[*pSrc >> 4];
                *pEnd++ = DEC2HEX[*pSrc & 0x0F];
            }
        }

        std::string sResult((char *)pStart, (char *)pEnd);
        delete [] pStart;
        return sResult;
    }

    std::string collection_dir(const std::string& colname)
    {
        std::string res;

        if (boost::algorithm::ends_with(colname, ".emulecollection"))
        {
            res = boost::algorithm::replace_all_copy(
                colname.substr(0, colname.find_last_of("-")), "-", "\\");
        }

        return res;
    }

    void extract_addresses(const std::string& line, filter& f, error_code& ec)
    {
        int stage = 0;
        boost::char_separator<char> dash_sep("-");
        boost::tokenizer<boost::char_separator<char> > tokens(line, dash_sep);

        BOOST_FOREACH(std::string adr, tokens)
        {
            adr.erase(std::remove_if(adr.begin(), adr.end(), isspace), adr.end());

            switch(stage)
            {
            case 0:
                f.begin = ip::address::from_string(adr, ec);
                break;
            case 1:
                f.end = ip::address::from_string(adr, ec);
                break;
            default:
                ec = errors::lines_syntax_error;
                break;
            }

            if (ec)
                break;

            ++stage;
        }

        if (!ec && stage != 2)
            ec = errors::lines_syntax_error;
    }

    filter datline2filter(const std::string& line, error_code& ec)
    {
        filter f;

        if (!line.empty() && line.at(0) != '#')
        {
            int stage = 0;
            boost::char_separator<char> comma_sep(",");
            boost::tokenizer<boost::char_separator<char> > tokens(line, comma_sep);

            BOOST_FOREACH(const std::string& str, tokens)
            {
                switch(stage)
                {
                    case 0:
                        extract_addresses(str, f, ec);
                        break;
                    case 1:
                    {
                        std::string level = str;
                        level.erase(std::remove_if(level.begin(), level.end(), isspace), level.end());

                        try
                        {
                            f.level = boost::lexical_cast<int>(level);
                        }
                        catch(boost::bad_lexical_cast &)
                        {
                            ec = errors::levels_value_error;
                        }

                        break;
                    }
                    case 2:
                        f.comment = str;
                        break;
                    default:
                        ec = errors::lines_syntax_error;
                        break;
                }

                ++stage;
            }

            // check we processed all elements
            if (!ec && stage != 3)
                ec = errors::lines_syntax_error;
        }
        else
        {
            ec = errors::empty_or_commented_line;
        }

        DBG("filter begin: " << f.begin.to_string());
        DBG("filter end  : " << f.end.to_string());
        DBG("filter level: " << f.level);
        DBG("filter comm : " << f.comment);
        return f;
    }
}
