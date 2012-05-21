
#ifndef __LIBED2K_SEARCH__
#define __LIBED2K_SEARCH__

#include "libed2k/packet_struct.hpp"

namespace libed2k
{
#define SEARCH_REQ_ELEM_LENGTH 20
#define SEARCH_REQ_QUERY_LENGTH 450
#define SEARCH_REQ_ELEM_COUNT  30
    /**
      * @param nMinSize - min size for result files, zero disable this parameter
      * @param nMaxSize - max size for result filesm zeto disable this parameter
      * @param nSourcesCount - min sources count, zero disable this parameter
      * @param strFileType - file type from file.hpp ED2KFTSTR_AUDIO,....,
      *        empty string disable this parameter
      * @param strFileExtension - file extension, empty string disable this parameter
      * @param strQuery - user expression with or without logical operators and
      *        quotation marks("), can not be empty
     */
    extern search_request generateSearchRequest(
        boost::uint64_t nMinSize, boost::uint64_t nMaxSize,
        unsigned int nSourcesCount, 
        unsigned int nCompleteSourcesCount,
        const std::string& strFileType,
        const std::string& strFileExtension,
        const std::string& strCodec,
        unsigned int nMediaLength,
        unsigned int nMediaBitrate,
        const std::string& strQuery);

    /**
     * @param md4_hash - file hash for search related files
     */
    extern search_request generateSearchRequest(const md4_hash& hFile);

}

#endif
