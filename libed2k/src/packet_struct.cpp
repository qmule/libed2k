#include "packet_struct.hpp"

namespace libed2k
{

    const char* packetToString(proto_type protocol)
    {
        static const char* pchUnknown = "Unknown packet";
        static const char* chData[] =
        {
                "00 DON'T USED",
                "OP_LOGINREQUEST",
                "02 DON'T USED",
                "03 DON'T USED",
                "04 DON'T USED",
                "OP_REJECT",
                "06 DON'T USED",
                "07 DON'T USED",
                "08 DON'T USED",
                "09 DON'T USED",
                "0A DON'T USED",
                "0B DON'T USED",
                "0C DON'T USED",
                "0D DON'T USED",
                "0E DON'T USED",
                "0F DON'T USED",
                "10 DON'T USED",
                "11 DON'T USED",
                "12 DON'T USED",
                "13 DON'T USED",
                "OP_GETSERVERLIST", //14
                "OP_OFFERFILES",
                "OP_SEARCHREQUEST",
                "17 DON'T USED",
                "OP_DISCONNECT",
                "OP_GETSOURCES",
                "OP_SEARCH_USER",
                "1B DON'T USED",
                "OP_CALLBACKREQUEST",
                "OP_QUERY_CHATS",
                "OP_CHAT_MESSAGE",
                "OP_JOIN_ROOM",
                "20 DON'T USED",
                "OP_QUERY_MORE_RESULT",
                "22 DON'T USED",
                "OP_GETSOURCES_OBFU",
                "24 DON'T USED",
                "25 DON'T USED",
                "26 DON'T USED",
                "27 DON'T USED",
                "28 DON'T USED",
                "29 DON'T USED",
                "2A DON'T USED",
                "2B DON'T USED",
                "2C DON'T USED",
                "2D DON'T USED",
                "2E DON'T USED",
                "2F DON'T USED",
                "30 DON'T USED",
                "31 DON'T USED",
                "OP_SERVERLIST",
                "OP_SEARCHRESULT",
                "OP_SERVERSTATUS",
                "OP_CALLBACKREQUESTED",
                "OP_CALLBACK_FAIL",
                "37 DON'T USED",
                "OP_SERVERMESSAGE",
                "OP_CHAT_ROOM_REQUEST",
                "OP_CHAT_BROADCAST",
                "OP_CHAT_USER_JOIN",
                "OP_CHAT_USER_LEAVE",
                "OP_CHAT_USER",
                "3E DON'T USED",
                "3F DON'T USED",
                "OP_IDCHANGE",
                "OP_SERVERIDENT",
                "OP_FOUNDSOURCES",
                "OP_USERS_LIST",
                "OP_FOUNDSOURCES_OBFU",
                "45 DON'T USED",
                "OP_SENDINGPART",
                "OP_REQUESTPARTS",
                "OP_FILEREQANSNOFIL",
                "OP_END_OF_DOWNLOAD",
                "OP_ASKSHAREDFILES",
                "OP_ASKSHAREDFILESANSWER",
                "OP_HELLOANSWER",
                "OP_CHANGE_CLIENT_ID",//         = 0x4D, // <ID_old 4><ID_new 4> // Unused for sending
                "OP_MESSAGE",
                "OP_SETREQFILEID",
                "OP_FILESTATUS",
                "OP_HASHSETREQUEST",
                "OP_HASHSETANSWER",
                "53 DON'T USED",
                "OP_STARTUPLOADREQ",
                "OP_ACCEPTUPLOADREQ",
                "OP_CANCELTRANSFER",
                "OP_OUTOFPARTREQS",
                "OP_REQUESTFILENAME",
                "OP_REQFILENAMEANSWER",
                "5A DON'T USED",
                "OP_CHANGE_SLOT",
                "OP_QUEUERANK",
                "OP_ASKSHAREDDIRS", //            = 0x5D, // (null)
                "OP_ASKSHAREDFILESDIR",
                "OP_ASKSHAREDDIRSANS",
                "OP_ASKSHAREDFILESDIRANS",
                "OP_ASKSHAREDDENIEDANS"//       = 0x61  // (null)
        };

        if (protocol < sizeof(chData)/sizeof(chData[0]))
        {
            return (chData[static_cast<unsigned int>(protocol)]);
        }

        return (pchUnknown);
    }


    const char* toString(search_request_entry::SRE_Operation so)
    {
        static const char* chOperations[] =
                {
                        "AND",
                        "OR",
                        "NOT"
                };

        return (chOperations[so]);
    }

    net_identifier::net_identifier() : m_nIP(0), m_nPort(0)
    {

    }

    net_identifier::net_identifier(boost::uint32_t nIP, boost::uint16_t nPort) : m_nIP(nIP), m_nPort(nPort)
    {

    }

    void net_identifier::dump() const
    {
        DBG("net_identifier::dump(IP=" << m_nIP << " port=" << m_nPort << ")");
    }

    void search_file_entry::dump() const
    {
        DBG("search_file_entry::dump");
        m_hFile.dump();
        m_network_point.dump();
        m_list.dump();
    }

    shared_file_entry::shared_file_entry()
    {
    }

    shared_file_entry::shared_file_entry(const md4_hash& hFile, boost::uint32_t nFileId, boost::uint16_t nPort) :
            m_hFile(hFile),
            m_network_point(nFileId, nPort)
    {
    }

    void shared_file_entry::dump() const
    {
        m_hFile.dump();
        m_network_point.dump();
    }

    void found_file_sources::dump() const
    {
        m_hFile.dump();
        m_sources.dump();
    }

    // special meta tag types
    const tg_type SEARCH_TYPE_BOOL      = '\x00';
    const tg_type SEARCH_TYPE_STR       = '\x01';
    const tg_type SEARCH_TYPE_STR_TAG   = '\x02';
    const tg_type SEARCH_TYPE_UINT32    = '\x03';
    const tg_type SEARCH_TYPE_UINT64    = '\x08';

    search_request_entry::search_request_entry(SRE_Operation soper)
    {
        m_type      = SEARCH_TYPE_BOOL;
        m_operator  = soper;
    }

    search_request_entry::search_request_entry(const std::string& strValue)
    {
        m_type      = SEARCH_TYPE_STR;
        m_strValue  = strValue;
    }

    search_request_entry::search_request_entry(tg_type nMetaTagId, const std::string& strValue)
    {
        m_type      = SEARCH_TYPE_STR_TAG;
        m_strValue  = strValue;
        m_meta_type = nMetaTagId;
    }

    search_request_entry::search_request_entry(const std::string strMetaTagName, const std::string& strValue)
    {
        m_type          = SEARCH_TYPE_STR_TAG;
        m_strValue      = strValue;
        m_strMetaName   = strMetaTagName;
    }

    search_request_entry::search_request_entry(tg_type nMetaTagId, boost::uint8_t nOperator, boost::uint64_t nValue)
    {
        m_meta_type = nMetaTagId;
        m_operator  = nOperator;

        if (nValue > 0xFFFFFFFFL)
        {
            m_nValue64  = nValue;
            m_type      = SEARCH_TYPE_UINT64;
        }
        else
        {
            m_nValue32  = static_cast<boost::uint32_t>(nValue);
            m_type      = SEARCH_TYPE_UINT32;
        }
    }

    search_request_entry::search_request_entry(const std::string& strMetaTagName, boost::uint8_t nOperator, boost::uint64_t nValue)
    {
        m_strValue  = strMetaTagName;
        m_operator  = nOperator;

        if (nValue > 0xFFFFFFFFL)
        {
            m_nValue64  = nValue;
            m_type      = SEARCH_TYPE_UINT64;
        }
        else
        {
            m_nValue32  = static_cast<boost::uint32_t>(nValue);
            m_type      = SEARCH_TYPE_UINT32;
        }
    }

    void search_request_entry::save(archive::ed2k_oarchive& ar)
    {
        ar & m_type;

        if (m_type == SEARCH_TYPE_BOOL)
        {
            DBG("write: " << toString(static_cast<search_request_entry::SRE_Operation>(m_operator)));
            // it is bool operator
            ar & m_operator;
            return;
        }

        if (m_type == SEARCH_TYPE_STR || m_type == SEARCH_TYPE_STR_TAG)
        {
            DBG("write string: " << m_strValue);
            // it is string value
            boost::uint16_t nSize = static_cast<boost::uint16_t>(m_strValue.size());
            ar & nSize;
            ar & m_strValue;

            boost::uint16_t nMetaTagSize;

            // write meta tag if it exists
            if (m_strMetaName.is_initialized())
            {
                nMetaTagSize = m_strMetaName.get().size();
                ar & nMetaTagSize;
                ar & m_strMetaName.get();
            }
            else if (m_meta_type.is_initialized())
            {
                nMetaTagSize = sizeof(m_meta_type.get());
                ar & nMetaTagSize;
                ar & m_meta_type.get();
            }

            return;
        }

        if (m_type == SEARCH_TYPE_UINT32 || m_type == SEARCH_TYPE_UINT64)
        {
            (m_type == SEARCH_TYPE_UINT32)?(ar & m_nValue32):(ar & m_nValue64);

            ar & m_operator;

            boost::uint16_t nMetaTagSize;

            // write meta tag if it exists
            if (m_strMetaName.is_initialized())
            {
                nMetaTagSize = m_strMetaName.get().size();
                ar & nMetaTagSize;
                ar & m_strMetaName.get();
            }
            else if (m_meta_type.is_initialized())
            {
                nMetaTagSize = sizeof(m_meta_type.get());
                ar & nMetaTagSize;
                ar & m_meta_type.get();
            }

            return;
        }

    }

    void search_request::add_entry(const search_request_entry& entry)
    {
        m_entries.push_back(entry);
    }

    global_server_state_res::global_server_state_res(size_t nMaxSize) :
            m_nChallenge(0),
            m_nUsersCount(0),
            m_nFilesCount(0),
            m_nCurrentMaxUsers(0),
            m_nSoftFiles(0),
            m_nHardFiles(0),
            m_nUDPFlags(0),
            m_nLowIdUsers(0),
            m_nUDPObfuscationPort(0),
            m_nTCPObfuscationPort(0),
            m_nServerUDPKey(0),
            m_nMaxSize(nMaxSize)
     {
     }

}
