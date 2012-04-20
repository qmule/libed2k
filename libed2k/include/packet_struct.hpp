#ifndef __PACKET_STRUCT__
#define __PACKET_STRUCT__

#include <vector>
#include <boost/cstdint.hpp>
#include "ctag.hpp"

namespace libed2k
{
    // protocol type
    typedef boost::uint8_t  proto_type;

    /**
      * print protocol type
     */
    const char* packetToString(proto_type protocol);

    // enum for client<->server messages
	enum OP_ClientToServerTCP
	{
	    OP_LOGINREQUEST             = 0x01, // <HASH 16><ID 4><PORT 2><1 Tag_set>
		OP_REJECT                   = 0x05, // (null)
		OP_GETSERVERLIST            = 0x14, // (null)client->server
		OP_OFFERFILES               = 0x15, // <count 4>(<HASH 16><ID 4><PORT 2><1 Tag_set>)[count]
		OP_SEARCHREQUEST            = 0x16, // <Query_Tree>
		OP_DISCONNECT               = 0x18, // (not verified)
		OP_GETSOURCES               = 0x19, // <HASH 16>
                                            // v2 <HASH 16><SIZE_4> (17.3) (mandatory on 17.8)
                                            // v2large <HASH 16><FILESIZE 4(0)><FILESIZE 8> (17.9) (large files only)
		OP_SEARCH_USER              = 0x1A, // <Query_Tree>
		OP_CALLBACKREQUEST          = 0x1C, // <ID 4>
	//  OP_QUERY_CHATS              = 0x1D, // (deprecated, not supported by server any longer)
	//  OP_CHAT_MESSAGE             = 0x1E, // (deprecated, not supported by server any longer)
	//  OP_JOIN_ROOM                = 0x1F, // (deprecated, not supported by server any longer)
		OP_QUERY_MORE_RESULT        = 0x21, // (null)
		OP_GETSOURCES_OBFU          = 0x23,
		OP_SERVERLIST               = 0x32, // <count 1>(<IP 4><PORT 2>)[count] server->client
		OP_SEARCHRESULT             = 0x33, // <count 4>(<HASH 16><ID 4><PORT 2><1 Tag_set>)[count]
		OP_SERVERSTATUS             = 0x34, // <USER 4><FILES 4>
		OP_CALLBACKREQUESTED        = 0x35, // <IP 4><PORT 2>
		OP_CALLBACK_FAIL            = 0x36, // (null notverified)
		OP_SERVERMESSAGE            = 0x38, // <len 2><Message len>
	//  OP_CHAT_ROOM_REQUEST        = 0x39, // (deprecated, not supported by server any longer)
	//  OP_CHAT_BROADCAST           = 0x3A, // (deprecated, not supported by server any longer)
	//  OP_CHAT_USER_JOIN           = 0x3B, // (deprecated, not supported by server any longer)
	//  OP_CHAT_USER_LEAVE          = 0x3C, // (deprecated, not supported by server any longer)
	//  OP_CHAT_USER                = 0x3D, // (deprecated, not supported by server any longer)
		OP_IDCHANGE                 = 0x40, // <NEW_ID 4>
		OP_SERVERIDENT              = 0x41, // <HASH 16><IP 4><PORT 2>{1 TAG_SET}
		OP_FOUNDSOURCES             = 0x42, // <HASH 16><count 1>(<ID 4><PORT 2>)[count]
		OP_USERS_LIST               = 0x43, // <count 4>(<HASH 16><ID 4><PORT 2><1 Tag_set>)[count]
		OP_FOUNDSOURCES_OBFU = 0x44    // <HASH 16><count 1>(<ID 4><PORT 2><obf settings 1>(UserHash16 if obf&0x08))[count]
	};

	/**
	  * supported protocols
	 */
	const proto_type    OP_EDONKEYHEADER        = '\xE3';
	const proto_type    OP_EDONKEYPROT          = OP_EDONKEYHEADER;
	const proto_type    OP_PACKEDPROT           = '\xD4';
	const proto_type    OP_EMULEPROT            = '\xC5';

    /**
      *  common container holder structure
      *  contains size_type for read it from archives and some container for data
     */
    template<typename size_type, class collection_type>
    struct container_holder
    {
        size_type       m_size;
        collection_type m_collection;
        typedef typename collection_type::iterator Iterator;
        typedef typename collection_type::value_type elem;

        template<typename Archive>
        void save(Archive& ar)
        {
            // save collection size in field with proprly size
            m_size = static_cast<size_type>(m_collection.size());
            ar & m_size;

            for(Iterator i = m_collection.begin(); i != m_collection.end(); ++i)
            {
                ar & *i;
            }

        }

        template<typename Archive>
        void load(Archive& ar)
        {
            ar & m_size;
            m_collection.resize(static_cast<size_t>(m_size));

            for (size_type i = 0; i < m_size; i++)
            {
                ar & m_collection[i];
            }
        }

        LIBED2K_SERIALIZATION_SPLIT_MEMBER()
    };

    /**
      * common libed2k packet header
      *
     */
#pragma pack(push,1)
    struct libed2k_header
    {
        typedef boost::uint32_t size_type;

        proto_type  m_protocol;             //!< protocol identifier
        size_type   m_size;                 //!< packet body size
        proto_type  m_type;                 //!< packet opcode
    };
#pragma pack(pop)

    // common protocol structures

    // file id/port in shared file entry with LowID client
    const boost::uint32_t FILE_COMPLETE_ID      = 0xfbfbfbfbU;
    const boost::uint16_t FILE_COMPLETE_PORT    = 0xfbfbU;
    const boost::uint32_t FILE_INCOMPLETE_ID    = 0xfcfcfcfcU;
    const boost::uint16_t FILE_INCOMPLETE_PORT  = 0xfcfcU;

    /**
      * common network object identifier in ed2k network
      *
     */
    struct net_identifier
    {
        boost::uint32_t m_nIP;          //!< client id or ip
        boost::uint16_t m_nPort;        //!< port

        template<typename Archive>
        void serialize(Archive& ar)
        {
            ar & m_nIP;
            ar & m_nPort;
        }
    };

    /**
      * shared file item structure in offer list
     */
    struct shared_file_entry
    {
        md4_hash                    m_hFile;    //!< md4 file hash
        boost::uint32_t             m_nFileId;  //!< client id for HighID
        boost::uint16_t             m_nPort;    //!< client port for HighID
        tag_list<boost::uint16_t>   m_list;     //!< file information list


        shared_file_entry();
        shared_file_entry(const md4_hash& hFile, boost::uint32_t nFileId, boost::uint16_t nPort);

        template<typename Archive>
        void serialize(Archive& ar)
        {
            ar & m_hFile;
            ar & m_nFileId;
            ar & m_nPort;
            ar & m_list;
        }
    };

    //!< client <-> server messages

    /**
      * login request structure - contain some info and 4 tag items
     */
    struct cs_login_request
    {
        md4_hash                    m_hClient;
        boost::uint32_t             m_nClientId;
        boost::uint16_t             m_nPort;
        tag_list<boost::uint32_t>   m_list;

        template<typename Archive>
        void serialize(Archive& ar)
        {
            ar & m_hClient;
            ar & m_nClientId;
            ar & m_nPort;
            ar & m_list;
        }
    };

    /**
      * server text message
     */
    struct server_message
    {
        boost::uint16_t m_nLength;
        std::string     m_strMessage;
        template<typename Archive>
        void serialize(Archive& ar)
        {
            ar & m_nLength;
            // allocate buffer if it needs
            if (m_strMessage.size() != m_nLength)
            {
                m_strMessage.resize(m_nLength);
            }

            ar & m_strMessage;
        }
    };

    /**
      * empty structure for get servers list from server
     */
    typedef container_holder<boost::uint8_t, std::vector<net_identifier> > server_list;
    typedef container_holder<boost::uint32_t, std::vector<shared_file_entry> > offer_files_list;

    /**
      * structure contain server IP/port, hash and some information tags
     */
    struct server_info_entry
    {
        md4_hash                    m_hServer;
        net_identifier              m_address;
        tag_list<boost::uint32_t>   m_list;
    };

    /**
      * structure contain one entry in searh result list
     */
    struct search_file_entry
    {
    	md4_hash					m_hFile;		//!< file hash
    	boost::uint32_t				m_nClientId;	//!< client id/ip
    	boost::uint16_t				m_nPort;		//!< port
    	tag_list<boost::uint32_t>	m_list;			//!< tag list with additional data - file name,size,sources ...
    };

    typedef container_holder<boost::uint32_t, std::vector<search_file_entry> > search_file_list;

    /**
      * this structure holds search requests to eDonkey server
      *
     */
    struct search_tree
    {

    };

    /**
      * request sources for file
     */
    struct get_sources_struct
    {
        md4_hash        m_hFile;        //!< file hash
        boost::uint32_t m_nSize;        //!< file size if file is not large file, otherwise is zero
        boost::uint16_t m_nLargeSize;   //!< file size if file is large file

        template<typename Archive>
        void serialize(Archive& ar)
        {
            ar & m_hFile;

            if (m_nLargeSize != 0)
            {
                m_nSize = 0;
                ar & m_nSize;
                ar & m_nLargeSize;
            }
            else
            {
                ar & m_nSize;
            }
        }
    };

    struct callback_req_struct
    {
        boost::uint32_t m_nUserId;
    };

    /**
      * server status structure
     */
    struct server_status_struct
    {
        boost::uint32_t m_nUserCount;
        boost::uint32_t m_nFilesCount;
    };

    /**
      * incoming callback request structure
     */
    struct callback_req_in
    {
        boost::uint32_t m_nIP;
        boost::uint32_t m_nPort;
        boost::uint8_t  m_nCryptOptions;
        md4_hash        m_hUser;

        template<typename Archive>
        void serialize(Archive& ar)
        {

        }
    };

    /**
      * file sources found structure
     */
    struct found_sources
    {
        md4_hash    m_hFile;
        container_holder<boost::uint8_t, std::vector<net_identifier> > m_sources;
    };

    /**
      * structure for get packet opcode from structure type
     */
    template<typename T>
    struct packet_type
    {
        static const proto_type value = 0;
    };

    template<> struct packet_type<cs_login_request>     { static const proto_type value = OP_LOGINREQUEST; };
    //OP_REJECT                   = 0x05
    //OP_GETSERVERLIST            = 0x14
    template<> struct packet_type<offer_files_list>     { static const proto_type value = OP_OFFERFILES;    };
    template<> struct packet_type<search_tree>          { static const proto_type value = OP_SEARCHREQUEST; };
    //OP_DISCONNECT               = 0x18, // (not verified)
    template<> struct packet_type<get_sources_struct>   { static const proto_type value = OP_GETSOURCES;    };
    //OP_SEARCH_USER              = 0x1A, // <Query_Tree>
    template<> struct packet_type<callback_req_struct>  { static const proto_type value = OP_CALLBACKREQUEST;};

    //  OP_QUERY_CHATS              = 0x1D, // (deprecated, not supported by server any longer)
    //  OP_CHAT_MESSAGE             = 0x1E, // (deprecated, not supported by server any longer)
    //  OP_JOIN_ROOM                = 0x1F, // (deprecated, not supported by server any longer)
    //        OP_QUERY_MORE_RESULT        = 0x21, // (null) // do not use
    //        OP_GETSOURCES_OBFU          = 0x23,           // do not use
    template<> struct packet_type<server_list>          { static const proto_type value = OP_SERVERLIST;    };
    template<> struct packet_type<search_file_list>     { static const proto_type value = OP_SEARCHRESULT;  };
    template<> struct packet_type<server_status_struct> { static const proto_type value = OP_SERVERSTATUS;  };
    template<> struct packet_type<callback_req_in>      { static const proto_type value = OP_CALLBACKREQUESTED; };

//            OP_CALLBACK_FAIL            = 0x36, // (null notverified)
    template<> struct packet_type<server_message>       { static const proto_type value = OP_SERVERMESSAGE; };

        //  OP_CHAT_ROOM_REQUEST        = 0x39, // (deprecated, not supported by server any longer)
        //  OP_CHAT_BROADCAST           = 0x3A, // (deprecated, not supported by server any longer)
        //  OP_CHAT_USER_JOIN           = 0x3B, // (deprecated, not supported by server any longer)
        //  OP_CHAT_USER_LEAVE          = 0x3C, // (deprecated, not supported by server any longer)
        //  OP_CHAT_USER                = 0x3D, // (deprecated, not supported by server any longer)
    //template<> struct packet_type<id_change>            { static const proto_type value = OP_IDCHANGE;    };
    template<> struct packet_type<server_info_entry>    { static const proto_type value = OP_SERVERIDENT; };
            //OP_SERVERIDENT              = 0x41, // <HASH 16><IP 4><PORT 2>{1 TAG_SET}
            // do not use
            //OP_FOUNDSOURCES             = 0x42, // <HASH 16><count 1>(<ID 4><PORT 2>)[count]
            //OP_USERS_LIST               = 0x43, // <count 4>(<HASH 16><ID 4><PORT 2><1 Tag_set>)[count]
            //OP_FOUNDSOURCES_OBFU = 0x44    // <HASH 16><count 1>(<ID 4><PORT 2><obf settings 1>(UserHash16 if obf&0x08))[count]
}


#endif //__PACKET_STRUCT__
