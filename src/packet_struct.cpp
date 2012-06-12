#include "libed2k/packet_struct.hpp"

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

    net_identifier::net_identifier() : m_nIP(0), m_nPort(0) {}
    net_identifier::net_identifier(boost::uint32_t nIP, boost::uint16_t nPort):
        m_nIP(nIP), m_nPort(nPort) {}
    net_identifier::net_identifier(const tcp::endpoint& ep):
        m_nIP(address2int(ep.address())), m_nPort(ep.port()) {}

    void net_identifier::dump() const
    {
        DBG("net_identifier::dump(IP=" << int2ipstr(m_nIP) << " port=" << m_nPort << ")");
    }

    void server_info_entry::dump() const
    {
        DBG("server_info_entry::dump");
        m_hServer.dump();
        m_network_point.dump();
        m_list.dump();
    }

    void search_result::dump() const
    {
        DBG("search_result::dump()");
        m_files.dump();

        if (m_more_results_avaliable != 0)
        {
            DBG("More results avaliable");
        }
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
        DBG("shared_file_entry::dump()");
        m_hFile.dump();
        m_network_point.dump();
        m_list.dump();
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

    bool search_request_entry::isOperand() const
    {
        return (m_type == SEARCH_TYPE_BOOL);
    }

    const std::string& search_request_entry::getStrValue() const
    {
        return (m_strValue);
    }

    boost::uint32_t search_request_entry::getInt32Value() const
    {
        return (m_nValue32);
    }

    boost::uint64_t search_request_entry::getInt64Value() const
    {
        return (m_nValue64);
    }

    boost::uint8_t search_request_entry::getOperator() const
    {
        return (m_operator);
    }

    void search_request_entry::dump() const
    {
        if (m_type == SEARCH_TYPE_BOOL)
        {
            DBG("BOOL OPERATOR: " << toString(static_cast<search_request_entry::SRE_Operation>(m_operator)));
        }
        else if (m_type == SEARCH_TYPE_STR || m_type == SEARCH_TYPE_STR_TAG)
        {
            DBG("STRING: " << m_strValue);
        }
        else if (m_type == SEARCH_TYPE_UINT32)
        {
            DBG("INT32: " << m_nValue32);
        }
        else if (m_type == SEARCH_TYPE_UINT64)
        {
            DBG("INT64: " << m_nValue64);
        }
        else
        {
            DBG("Something weird");
        }

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

    client_message::client_message() : m_nMsgLength(0)
    {}

    client_message::client_message(const std::string& strMessage)
    {
        m_nMsgLength = static_cast<boost::uint16_t>(strMessage.length());

        if (m_nMsgLength > CLIENT_MAX_MESSAGE_LENGTH)
        {
            m_nMsgLength = CLIENT_MAX_MESSAGE_LENGTH;

        }

        m_strMessage.assign(strMessage, 0, m_nMsgLength);
    }

    client_meta_packet::client_meta_packet(const client_message& cmessage)
    {
        m_message = cmessage;
        m_proto = get_proto_type(m_message);
    }

    client_meta_packet::client_meta_packet(const client_shared_files_request& frequest)
    {
        m_files_request = frequest;
        m_proto = get_proto_type(m_files_request);
    }

    client_meta_packet::client_meta_packet(const client_shared_directories_request& drequest)
    {
        m_directories_request = drequest;
        m_proto = get_proto_type(m_directories_request);
    }

    client_meta_packet::client_meta_packet(const client_shared_directory_files& frequest)
    {
        m_directory_files_request = frequest;
        m_proto = get_proto_type(m_directory_files_request);
    }

    client_meta_packet::client_meta_packet(const shared_files_list& flist)
    {
        m_files_list = flist;
        m_proto = get_proto_type(m_files_list);
    }

    peer_connection_options::peer_connection_options() : m_nVersion(0),
                 m_nModVersion(0),
                 m_nPort(0),
                 m_nUDPPort(0),
                 m_nBuddyUDP(0),
                 m_nClientVersion(0),
                 m_nCompatibleClient(0),
                 m_bOsInfoSupport(false),
                 m_bValueBasedTypeTags(false)
    {}

    misc_options::misc_options()
    {
        load(0);
    }

    misc_options::misc_options(boost::uint32_t opts)
    {
        load(opts);
    }

    void misc_options::load(boost::uint32_t opts)
    {
        m_nAICHVersion          = (opts >> (4*7+1)) & 0x07;
        m_nUnicodeSupport       = (opts >> 4*7) & 0x01;
        m_nUDPVer               = (opts >> 4*6) & 0x0f;
        m_nDataCompVer          = (opts >> 4*5) & 0x0f;
        m_nSupportSecIdent      = (opts >> 4*4) & 0x0f;
        m_nSourceExchange1Ver   = (opts >> 4*3) & 0x0f;
        m_nExtendedRequestsVer  = (opts >> 4*2) & 0x0f;
        m_nAcceptCommentVer     = (opts >> 4*1) & 0x0f;
        m_nNoViewSharedFiles    = (opts >> 1*2) & 0x01;
        m_nMultiPacket          = (opts >> 1*1) & 0x01;
        m_nSupportsPreview      = (opts >> 1*0) & 0x01;
    }

    misc_options2::misc_options2() : m_options(0)
    {

    }

    misc_options2::misc_options2(boost::uint32_t opts) : m_options(opts)
    {
    }

    void misc_options2::load(boost::uint32_t opts)
    {
        m_options = opts;
    }

    bool misc_options2::support_captcha() const
    {
        return ((m_options >> CAPTHA_OFFSET) & 0x01);
    }

    bool misc_options2::support_source_ext2() const
    {
        return ((m_options >> SRC_EXT_OFFSET) & 0x01);
    }

    bool misc_options2::support_ext_multipacket() const
    {
        return ((m_options >> MULTIP_OFFSET) & 0x01);
    }

    bool misc_options2::support_large_files() const
    {
        return ((m_options >> LARGE_FILE_OFFSET) & 0x01);
    }

    void misc_options2::set_captcha()
    {
        boost::uint32_t n = 1;
        m_options |= n << CAPTHA_OFFSET;
    }

    void misc_options2::set_source_ext2()
    {
        boost::uint32_t n = 1;
        m_options |= n << SRC_EXT_OFFSET;
    }

    void misc_options2::set_ext_multipacket()
    {
        boost::uint32_t n = 1;
        m_options |= n << MULTIP_OFFSET;
    }

    void misc_options2::set_large_files()
    {
        boost::uint32_t n = 1;
        m_options |= n << LARGE_FILE_OFFSET;
    }

    boost::uint32_t misc_options2::generate() const
    {
        return (m_options);
    }

    boost::uint32_t misc_options::generate() const
    {
        return  ((m_nAICHVersion           << ((4*7)+1)) |
                (m_nUnicodeSupport        << 4*7) |
                (m_nUDPVer                << 4*6) |
                (m_nDataCompVer           << 4*5) |
                (m_nSupportSecIdent       << 4*4) |
                (m_nSourceExchange1Ver    << 4*3) |
                (m_nExtendedRequestsVer   << 4*2) |
                (m_nAcceptCommentVer      << 4*1) |
                (m_nNoViewSharedFiles     << 1*2) |
                (m_nMultiPacket           << 1*1) |
                (m_nSupportsPreview       << 1*0));
    }


}
