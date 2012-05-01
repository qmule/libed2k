
#include "util.hpp"
#include "zlib.h"

namespace libed2k 
{
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

}
