
#include <algorithm>
#include "util.hpp"
#include "zlib.h"
#include "file.hpp"

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

    search_request_entry::SRE_Operation string2OperType(const std::string& strMark)
    {
        static const std::pair<std::string, search_request_entry::SRE_Operation> strMarks[] =
        {
                std::make_pair(std::string("AND"),  search_request_entry::SRE_AND),
                std::make_pair(std::string("OR"),   search_request_entry::SRE_OR),
                std::make_pair(std::string("NOT"),  search_request_entry::SRE_NOT)
        };

        for (size_t n = 0; n < sizeof(strMarks)/sizeof(strMarks[0]); n++)
        {
            if (strMark == strMarks[n].first)
            {
                return strMarks[n].second;
            }
        }

        return (search_request_entry::SRE_END);
    }

    bool is_quote(char c)
    {
        return (c == '"');
    }

    search_request generateSearchRequest(boost::uint64_t nMinSize,
                boost::uint64_t nMaxSize,
                unsigned int nSourcesCount,
                const std::string& strFileType,
                const std::string& strFileExtension,
                const std::string& strQuery)
    {
        search_request vPrefResult;
        std::vector<search_request_entry> vResult;

        if (strQuery.empty())
        {
            return (vPrefResult);
        }

        if (nMinSize != 0)
        {
            vResult.push_back(search_request_entry(FT_FILESIZE, ED2K_SEARCH_OP_GREATER, nMinSize));
        }

        if (nMaxSize != 0)
        {
            if (!vResult.empty())
            {
                vResult.push_back(search_request_entry(search_request_entry::SRE_AND));
            }

            vResult.push_back(search_request_entry(FT_FILESIZE, ED2K_SEARCH_OP_LESS, nMaxSize));
        }

        if (nSourcesCount != 0)
        {
            if (!vResult.empty())
            {
                vResult.push_back(search_request_entry(search_request_entry::SRE_AND));
            }

            vResult.push_back(search_request_entry(FT_SOURCES, ED2K_SEARCH_OP_GREATER, nSourcesCount));
        }

        if (!strFileType.empty())
        {
            if (!vResult.empty())
            {
                vResult.push_back(search_request_entry(search_request_entry::SRE_AND));
            }

            if ((strFileType == ED2KFTSTR_ARCHIVE) || (strFileType == ED2KFTSTR_CDIMAGE))
            {
                vResult.push_back(search_request_entry(FT_FILETYPE, ED2KFTSTR_PROGRAM));
            }
            else
            {
                vResult.push_back(search_request_entry(FT_FILETYPE, strFileType)); // I don't check this value!
            }
        }


        if (!strFileExtension.empty())
        {
            if (!vResult.empty())
            {
                vResult.push_back(search_request_entry(search_request_entry::SRE_AND));
            }

            vResult.push_back(search_request_entry(FT_FILEFORMAT, strFileExtension)); // I don't check this value!
        }


        // extension

        bool bVerbatim = false; // verbatim mode collect all characters
        std::string strItem;

        for (std::string::size_type n = 0; n < strQuery.size(); n++)
        {
            char c = strQuery.at(n);

            switch(strQuery.at(n))
            {
                case ' ':
                {
                    if (bVerbatim)
                    {
                        strItem += c;
                    }
                    else if (!strItem.empty())
                    {
                        search_request_entry::SRE_Operation so = string2OperType(strItem);

                        if (so != search_request_entry::SRE_END)
                        {
                            // add boolean operator
                            if (vResult.empty() || vResult.back().isOperand())
                            {
                               throw libed2k_exception(errors::operator_incorrect_place);
                            }
                            else
                            {
                                vResult.push_back(search_request_entry(so));
                            }
                        }
                        else
                        {
                            strItem.erase(std::remove_if(strItem.begin(), strItem.end(), is_quote), strItem.end()); // remove all quotation marks

                            if (!vResult.empty() && !vResult.back().isOperand())
                            {
                                // explicitly add AND operand when we have two splitted strings
                                vResult.push_back(search_request_entry(search_request_entry::SRE_AND));
                            }

                            vResult.push_back(strItem);
                        }

                        strItem.clear();

                    }

                    break;
                }
                case '"':
                    bVerbatim = !bVerbatim; // change verbatim status and add this character
                default:
                    strItem += c;
                    break;
            }

        }

        if (bVerbatim)
        {
            throw libed2k_exception(errors::unclosed_quotation_mark);
        }

        if (!strItem.empty())
        {
            // add last item
            search_request_entry::SRE_Operation so = string2OperType(strItem);
            if (so != search_request_entry::SRE_END)
            {
                throw libed2k_exception(errors::operator_incorrect_place);
            }
            else
            {
                if (!vResult.empty() && !vResult.back().isOperand())
                {
                    // explicitly add AND operand when we have two splitted strings
                    vResult.push_back(search_request_entry(search_request_entry::SRE_AND));
                }

                strItem.erase(std::remove_if(strItem.begin(), strItem.end(), is_quote), strItem.end()); // remove all quotation marks
                vResult.push_back(search_request_entry(strItem));
            }
        }

        std::vector<search_request_entry>::iterator itr = vResult.begin();

        while(itr != vResult.end())
        {
            if (itr->isOperand())
            {
                vPrefResult.push_front(*itr);
            }
            else
            {
                vPrefResult.push_back(*itr);
            }

            ++itr;
        }

        return (vPrefResult);
    }

    extern search_request generateSearchRequest(const md4_hash& hFile)
    {
        search_request vPrefResult;
        vPrefResult.push_back(search_request_entry(std::string("related::") + hFile.toString()));
        return vPrefResult;
    }

}
