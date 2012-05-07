
#include <algorithm>
#include <libtorrent/utf8.hpp>
#include <libtorrent/escape_string.hpp>

#ifndef WIN32
#include <iconv.h>
#endif
#include <locale.h>

#include "util.hpp"
#include "zlib.h"
#include "file.hpp"

namespace libed2k 
{
    #define CHECK_BOM(size, x) ((size >= 3)  && (x[0] == (char)0xEF) && (x[1] == (char)0xBB) && (x[2] == (char)0xBF))

    std::string bom_filter(const std::string& s)
    {
        if (CHECK_BOM(s.size(), s))
        {
            return s.substr(3);
        }

        return s;
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
#if 0
    std::wstring convert_to_wstring(std::string const& s)
    {
        std::wstring ret;
        int result = libtorrent::utf8_wchar(s, ret);
#ifndef _WIN32
        return ret;
#else
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

    std::string convert_from_filesystem(const std::wstring& w)
    {
        std::string str;
        libtorrent::wchar_utf8(w, str);
        return (str);
    }
#endif
#endif

    std::string convert_to_native(const std::string& s)
    {
        return (libtorrent::convert_to_native(s));
    }


#ifndef WIN32
    std::string convert_from_native(std::string const& s)
    {
        // only one thread can use this handle at a time
        static boost::mutex iconv_mutex;
        boost::mutex::scoped_lock l(iconv_mutex);

        // the empty string represents the local dependent encoding
        static iconv_t iconv_handle = iconv_open("UTF-8", "");
        if (iconv_handle == iconv_t(-1)) return s;
        std::string ret;
        size_t insize = s.size();
        size_t outsize = insize * 4;
        ret.resize(outsize);
        char const* in = s.c_str();
        char* out = &ret[0];
        // posix has a weird iconv signature. implementations
        // differ on what this signature should be, so we use
        // a macro to let config.hpp determine it
        size_t retval = iconv(iconv_handle, TORRENT_ICONV_ARG &in, &insize,
            &out, &outsize);
        if (retval == (size_t)-1) return s;
        // if this string has an invalid utf-8 sequence in it, don't touch it
        if (insize != 0) return s;
        // not sure why this would happen, but it seems to be possible
        if (outsize > s.size() * 4) return s;
        // outsize is the number of bytes unused of the out-buffer
        BOOST_ASSERT(ret.size() >= outsize);
        ret.resize(ret.size() - outsize);
        return ret;
    }

#else
    std::string convert_from_native(std::string const& s)
    {
#ifndef BOOST_NO_EXCEPTIONS
        try
        {
#endif
            // calculate wstring size for output
            std::size_t size = mbstowcs(0, s.c_str(), 0);
            if (size == std::size_t(-1)) return s;

            std::wstring ws;
            ws.resize(size);
            // convert
            size = mbstowcs(&ws[0], s.c_str(), size + 1);
            if (size == std::size_t(-1)) return s;

            std::string ret;
            libtorrent::wchar_utf8(ws, ret);
            return (ret);

#ifndef BOOST_NO_EXCEPTIONS
        }
        catch(std::exception)
        {
            return s;
        }
#endif
    }

#endif

}
