#include <stack>
#include "libed2k/search.hpp"
#include "libed2k/file.hpp"

namespace libed2k
{

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

    void item_append(std::vector<search_request_entry>& dst, const search_request_entry& sre)
    {
        if (!sre.isLogic())
        {
            if (!dst.empty())
            {
                if ((!dst.back().isOperator() && !sre.isOperator()) ||                                                              // xxx xxx
                (!dst.back().isOperator() && sre.getOperator() == search_request_entry::SRE_OBR) ||                                 // xxx (
                (dst.back().getOperator() == search_request_entry::SRE_CBR && !sre.isOperator()) ||                                 // ) xxx
                (dst.back().getOperator() == search_request_entry::SRE_CBR && sre.getOperator() == search_request_entry::SRE_OBR))  // ) (
                {
                    dst.push_back(search_request_entry(search_request_entry::SRE_AND));
                }

                if (dst.back().getOperator() == search_request_entry::SRE_OBR && sre.getOperator() == search_request_entry::SRE_CBR)
                {
                    throw libed2k_exception(errors::empty_brackets);
                }
            }
        }

        dst.push_back(sre);
    }

    search_request generateSearchRequest(boost::uint64_t nMinSize,
                boost::uint64_t nMaxSize,
                unsigned int nSourcesCount,
                unsigned int nCompleteSourcesCount,
                const std::string& strFileType,
                const std::string& strFileExtension,
                const std::string& strCodec,
                unsigned int nMediaLength,
                unsigned int nMediaBitrate,
                const std::string& strQuery)
    {

#define ADD_AND() \
        if (!vResult.empty())\
        {\
            vResult.push_back(search_request_entry(search_request_entry::SRE_AND));\
        }

        // check parameters
        if (strFileType.length() > SEARCH_REQ_ELEM_LENGTH ||
                strFileExtension.length() > SEARCH_REQ_ELEM_LENGTH ||
                strCodec.length() > SEARCH_REQ_ELEM_LENGTH ||
                strQuery.length() > SEARCH_REQ_QUERY_LENGTH)
        {
            throw libed2k_exception(errors::input_string_too_large);
        }

        search_request vPrefResult;
        std::vector<search_request_entry> vResult;

        if (strQuery.empty())
        {
            return (vPrefResult);
        }

        // for users pass any parameters
        if (strFileType == ED2KFTSTR_USER)
        {
            vResult.push_back(search_request_entry("'+++USERNICK+++'"));
            vResult.push_back(search_request_entry(search_request_entry::SRE_AND));
        }
        else if (strFileType == ED2KFTSTR_FOLDER) // for folders we search emule collections exclude ed2k links
        {
            vResult.push_back(search_request_entry(FT_FILETYPE, ED2KFTSTR_EMULECOLLECTION));
            vResult.push_back(search_request_entry(search_request_entry::SRE_NOT));
            vResult.push_back(search_request_entry("ED2K:\\"));
        }
        else
        {
            if (!strFileType.empty())
            {
                if ((strFileType == ED2KFTSTR_ARCHIVE) || (strFileType == ED2KFTSTR_CDIMAGE))
                {
                    vResult.push_back(search_request_entry(FT_FILETYPE, ED2KFTSTR_PROGRAM));
                }
                else
                {
                    vResult.push_back(search_request_entry(FT_FILETYPE, strFileType)); // I don't check this value!
                }
            }

            // if type is not folder - process file parameters now
            if (strFileType != ED2KFTSTR_EMULECOLLECTION)
            {

                if (nMinSize != 0)
                {
                    ADD_AND()
                    vResult.push_back(search_request_entry(FT_FILESIZE, ED2K_SEARCH_OP_GREATER, nMinSize));
                }

                if (nMaxSize != 0)
                {
                    ADD_AND()
                    vResult.push_back(search_request_entry(FT_FILESIZE, ED2K_SEARCH_OP_LESS, nMaxSize));
                }

                if (nSourcesCount != 0)
                {
                    ADD_AND()
                    vResult.push_back(search_request_entry(FT_SOURCES, ED2K_SEARCH_OP_GREATER, nSourcesCount));
                }

                if (nCompleteSourcesCount != 0)
                {
                    ADD_AND()
                    vResult.push_back(search_request_entry(FT_COMPLETE_SOURCES, ED2K_SEARCH_OP_GREATER, nCompleteSourcesCount));
                }

                if (!strFileExtension.empty())
                {
                    ADD_AND()
                    vResult.push_back(search_request_entry(FT_FILEFORMAT, strFileExtension)); // I don't check this value!
                }

                if (!strCodec.empty())
                {
                    ADD_AND()
                    vResult.push_back(search_request_entry(FT_MEDIA_CODEC, strCodec)); // I don't check this value!
                }

                if (nMediaLength != 0)
                {
                    ADD_AND()
                    vResult.push_back(search_request_entry(FT_MEDIA_LENGTH, ED2K_SEARCH_OP_GREATER_EQUAL, nMediaLength)); // I don't check this value!
                }

                if (nMediaBitrate != 0)
                {
                    ADD_AND()
                    vResult.push_back(search_request_entry(FT_MEDIA_BITRATE, ED2K_SEARCH_OP_GREATER_EQUAL, nMediaBitrate)); // I don't check this value!
                }


            }
        }

        size_t nBeforeCount = vResult.size();
        // extension

        bool bVerbatim = false; // verbatim mode collect all characters
        std::string strItem;

        for (std::string::size_type n = 0; n < strQuery.size(); n++)
        {
            char c = strQuery.at(n);

            switch(c)
            {
                case ' ':
                case '(':
                case ')':
                {
                    if (bVerbatim)
                    {
                        strItem += c;
                    }
                    else if (!strItem.empty())
                    {
                        DBG("item:{" << strItem << "}c{" << c << "}");
                        search_request_entry::SRE_Operation so = string2OperType(strItem);

                        if (so != search_request_entry::SRE_END)
                        {
                            // add boolean operator
                            if (vResult.empty() || vResult.back().isLogic() || (c == ')'))
                            {
                                // operator in begin, operator before previous operator and operator before close bracket is error
                                throw libed2k_exception(errors::operator_incorrect_place);
                            }
                            else
                            {
                                item_append(vResult, search_request_entry(so));
                            }
                        }
                        else
                        {
                            strItem.erase(std::remove_if(strItem.begin(), strItem.end(), is_quote), strItem.end()); // remove all quotation marks
                            item_append(vResult, strItem);
                        }

                        strItem.clear();

                    }

                    if (c == '(')
                    {
                        item_append(vResult, search_request_entry(search_request_entry::SRE_OBR));
                    }

                    if (c == ')')
                    {
                        item_append(vResult, search_request_entry(search_request_entry::SRE_CBR));
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
                if (!vResult.empty() && !vResult.back().isLogic())
                {
                    // explicitly add AND operand when we have two splitted strings
                    vResult.push_back(search_request_entry(search_request_entry::SRE_AND));
                }

                strItem.erase(std::remove_if(strItem.begin(), strItem.end(), is_quote), strItem.end()); // remove all quotation marks
                vResult.push_back(search_request_entry(strItem));
            }
        }

        if (vResult.size() - nBeforeCount > SEARCH_REQ_ELEM_COUNT)
        {
            throw libed2k_exception(errors::search_expression_too_complex);
        }

        std::stack<search_request_entry> operators_stack;

        // temp dump
        for(std::vector<search_request_entry>::const_reverse_iterator itr = vResult.rbegin();
                itr != vResult.rend(); ++itr)
        {
            DBG("value{" << itr->getStrValue() << "} oper {" << sre_operation2string(itr->getOperator()) << "}");
        }

        for(std::vector<search_request_entry>::const_reverse_iterator itr = vResult.rbegin();
                itr != vResult.rend(); ++itr)
        {
            if (itr->isOperator())
            {
                if (itr->getOperator() == search_request_entry::SRE_OBR)
                {
                    if (operators_stack.empty())
                    {
                        throw libed2k_exception(errors::incorrect_brackets_count);
                    }

                    // roll up
                    while(operators_stack.top().getOperator() != search_request_entry::SRE_CBR)
                    {
                        vPrefResult.push_front(operators_stack.top());
                        operators_stack.pop();

                        if (operators_stack.empty())
                        {
                            throw libed2k_exception(errors::incorrect_brackets_count);
                        }
                    }

                    // pull close bracket entry
                    operators_stack.pop();
                    continue;
                }

                // we have normal operator and on stack top we have normal operator
                // prepare result - move operator from top to result and replace top
                if ((itr->getOperator() <= search_request_entry::SRE_NOT) &&
                        !operators_stack.empty() &&
                        (operators_stack.top().getOperator() <= search_request_entry::SRE_NOT))
                {
                    vPrefResult.push_front(operators_stack.top());
                    operators_stack.pop();
                }

                operators_stack.push(*itr);
            }
            else
            {
                vPrefResult.push_front(*itr);
            }
        }

        if (!operators_stack.empty())
        {
            if (operators_stack.top().getOperator() > search_request_entry::SRE_NOT)
            {
                throw libed2k_exception(errors::incorrect_brackets_count);
            }

            vPrefResult.push_front(operators_stack.top());
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
