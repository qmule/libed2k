
#include <algorithm>
#include <libtorrent/utf8.hpp>
#include "util.hpp"
#include "zlib.h"
#include "file.hpp"

namespace libed2k 
{
    std::string bitfield2string(const bitfield& bits)
    {
        std::stringstream str;
        for(size_t i = 0; i < bits.size(); ++i) str << bits[i];
        return str.str();
    }

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

    std::wstring convert_to_wstring(std::string const& s)
    {
        std::wstring ret;
#ifndef _WIN32
        return ret;
#else
        int result = libtorrent::utf8_wchar(s, ret);
        if (result == 0) return ret;

        ret.clear();
        const char* end = &s[0] + s.size();
        for (const char* i = &s[0]; i < end;)
        {
            wchar_t c = '.';
            int result = std::mbtowc(&c, i, end - i);
            if (result > 0) i += result;
            else ++i;
            ret += c;
        }
        return ret;
#endif
    }

#ifdef WIN32
    std::wstring convert_to_filesystem(const std::string& s)
    {
        std::wstring wstr;
        libtorrent::utf8_wchar(s, wstr);
        return (wstr);
    }
#endif

}
