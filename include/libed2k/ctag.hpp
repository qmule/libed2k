#ifndef __CTAG__
#define __CTAG__

#include <iostream>
#include <string>
#include <deque>
#include <cassert>

#include <boost/cstdint.hpp>
#include <boost/mem_fn.hpp>
#include "libed2k/log.hpp"
#include "libed2k/archive.hpp"
#include "libed2k/hasher.hpp"
#include "libed2k/size_type.hpp"

namespace libed2k{

#define CHECK_TAG_TYPE(x)\
if (!x)\
{\
  throw libed2k::libed2k_exception(libed2k::errors::incompatible_tag_getter);\
}

typedef boost::uint8_t tg_nid_type;
typedef boost::uint8_t tg_type;

const tg_nid_type FT_UNDEFINED          = 0x00u;    // undefined tag
const tg_nid_type FT_FILENAME           = 0x01u;    // <string>
const tg_nid_type FT_FILESIZE           = 0x02u;    // <uint32>
const tg_nid_type FT_FILESIZE_HI        = 0x3Au;    // <uint32>
const tg_nid_type FT_FILETYPE           = 0x03u;    // <string> or <uint32>
const tg_nid_type FT_FILEFORMAT         = 0x04u;    // <string>
const tg_nid_type FT_LASTSEENCOMPLETE   = 0x05u;    // <uint32>
const tg_nid_type FT_TRANSFERRED        = 0x08u;    // <uint32>
const tg_nid_type FT_GAPSTART           = 0x09u;    // <uint32>
const tg_nid_type FT_GAPEND             = 0x0Au;    // <uint32>
const tg_nid_type FT_PARTFILENAME       = 0x12u;    // <string>
const tg_nid_type FT_OLDDLPRIORITY      = 0x13u;    // Not used anymore
const tg_nid_type FT_STATUS             = 0x14u;    // <uint32>
const tg_nid_type FT_SOURCES            = 0x15u;    // <uint32>
const tg_nid_type FT_PERMISSIONS        = 0x16u;    // <uint32>
const tg_nid_type FT_OLDULPRIORITY      = 0x17u;    // Not used anymore
const tg_nid_type FT_DLPRIORITY         = 0x18u;    // Was 13
const tg_nid_type FT_ULPRIORITY         = 0x19u;    // Was 17
const tg_nid_type FT_KADLASTPUBLISHKEY  = 0x20u;    // <uint32>
const tg_nid_type FT_KADLASTPUBLISHSRC  = 0x21u;    // <uint32>
const tg_nid_type FT_FLAGS              = 0x22u;    // <uint32>
const tg_nid_type FT_DL_ACTIVE_TIME     = 0x23u;    // <uint32>
const tg_nid_type FT_CORRUPTEDPARTS     = 0x24u;    // <string>
const tg_nid_type FT_DL_PREVIEW         = 0x25u;
const tg_nid_type FT_KADLASTPUBLISHNOTES= 0x26u;    // <uint32>
const tg_nid_type FT_AICH_HASH          = 0x27u;
const tg_nid_type FT_FILEHASH           = 0x28u;
const tg_nid_type FT_COMPLETE_SOURCES   = 0x30u;    // nr. of sources which share a
const tg_nid_type FT_FAST_RESUME_DATA   = 0x31u;   // fast resume data array

// Kad search + some unused tags to mirror the ed2k ones.
const tg_nid_type   TAG_FILENAME        = '\x01';  // <string>
const tg_nid_type   TAG_FILESIZE        = '\x02';  // <uint32>
const tg_nid_type   TAG_FILESIZE_HI     = '\x3A';  // <uint32>
const tg_nid_type   TAG_FILETYPE        = '\x03';  // <string>
const tg_nid_type   TAG_FILEFORMAT      = '\x04';  // <string>
const tg_nid_type   TAG_COLLECTION      = '\x05';
const tg_nid_type   TAG_PART_PATH       = '\x06';  // <string>
const tg_nid_type   TAG_PART_HASH       = '\x07';
const tg_nid_type   TAG_COPIED          = '\x08';  // <uint32>
const tg_nid_type   TAG_GAP_START       = '\x09';  // <uint32>
const tg_nid_type   TAG_GAP_END         = '\x0A';  // <uint32>
const tg_nid_type   TAG_DESCRIPTION     = '\x0B';  // <string>
const tg_nid_type   TAG_PING            = '\x0C';
const tg_nid_type   TAG_FAIL            = '\x0D';
const tg_nid_type   TAG_PREFERENCE      = '\x0E';
const tg_nid_type   TAG_PORT            = '\x0F';
const tg_nid_type   TAG_IP_ADDRESS      = '\x10';
const tg_nid_type   TAG_VERSION         = '\x11';  // <string>
const tg_nid_type   TAG_TEMPFILE        = '\x12';  // <string>
const tg_nid_type   TAG_PRIORITY        = '\x13';  // <uint32>
const tg_nid_type   TAG_STATUS          = '\x14';  // <uint32>
const tg_nid_type   TAG_SOURCES         = '\x15';  // <uint32>
const tg_nid_type   TAG_AVAILABILITY    = '\x15';  // <uint32>
const tg_nid_type   TAG_PERMISSIONS     = '\x16';
const tg_nid_type   TAG_QTIME           = '\x16';
const tg_nid_type   TAG_PARTS           = '\x17';
const tg_nid_type   TAG_PUBLISHINFO     = '\x33';  // <uint32>
const tg_nid_type   TAG_MEDIA_ARTIST    = '\xD0';  // <string>
const tg_nid_type   TAG_MEDIA_ALBUM     = '\xD1';  // <string>
const tg_nid_type   TAG_MEDIA_TITLE     = '\xD2';  // <string>
const tg_nid_type   TAG_MEDIA_LENGTH    = '\xD3';  // <uint32> !!!
const tg_nid_type   TAG_MEDIA_BITRATE   = '\xD4';  // <uint32>
const tg_nid_type   TAG_MEDIA_CODEC     = '\xD5';  // <string>
const tg_nid_type   TAG_KADMISCOPTIONS  = '\xF2';  // <uint8>
const tg_nid_type   TAG_ENCRYPTION      = '\xF3';  // <uint8>
const tg_nid_type   TAG_FILERATING      = '\xF7';  // <uint8>
const tg_nid_type   TAG_BUDDYHASH       = '\xF8';  // <string>
const tg_nid_type   TAG_CLIENTLOWID     = '\xF9';  // <uint32>
const tg_nid_type   TAG_SERVERPORT      = '\xFA';  // <uint16>
const tg_nid_type   TAG_SERVERIP        = '\xFB';  // <uint32>
const tg_nid_type   TAG_SOURCEUPORT     = '\xFC';  // <uint16>
const tg_nid_type   TAG_SOURCEPORT      = '\xFD';  // <uint16>
const tg_nid_type   TAG_SOURCEIP        = '\xFE';  // <uint32>
const tg_nid_type   TAG_SOURCETYPE      = '\xFF';  // <uint8>

// complete version of the
// associated file (supported
// by eserver 16.46+) statistic

const tg_nid_type FT_PUBLISHINFO        = 0x33u;    // <uint32>
const tg_nid_type FT_ATTRANSFERRED      = 0x50u;    // <uint32>
const tg_nid_type FT_ATREQUESTED        = 0x51u;    // <uint32>
const tg_nid_type FT_ATACCEPTED         = 0x52u;    // <uint32>
const tg_nid_type FT_CATEGORY           = 0x53u;    // <uint32>
const tg_nid_type FT_ATTRANSFERREDHI    = 0x54u;    // <uint32>
const tg_nid_type FT_MEDIA_ARTIST       = 0xD0u;    // <string>
const tg_nid_type FT_MEDIA_ALBUM        = 0xD1u;    // <string>
const tg_nid_type FT_MEDIA_TITLE        = 0xD2u;    // <string>
const tg_nid_type FT_MEDIA_LENGTH       = 0xD3u;    // <uint32> !!!
const tg_nid_type FT_MEDIA_BITRATE      = 0xD4u;    // <uint32>
const tg_nid_type FT_MEDIA_CODEC        = 0xD5u;    // <string>
const tg_nid_type FT_FILERATING         = 0xF7u;    // <uint8>


// server tags
const tg_nid_type ST_SERVERNAME         = 0x01u; // <string>
// Unused (0x02-0x0A)
const tg_nid_type ST_DESCRIPTION        = 0x0Bu; // <string>
const tg_nid_type ST_PING               = 0x0Cu; // <uint32>
const tg_nid_type ST_FAIL               = 0x0Du; // <uint32>
const tg_nid_type ST_PREFERENCE         = 0x0Eu; // <uint32>
        // Unused (0x0F-0x84)
const tg_nid_type ST_DYNIP              = 0x85u;
const tg_nid_type ST_LASTPING_DEPRECATED= 0x86u; // <uint32> // DEPRECATED, use 0x90
const tg_nid_type ST_MAXUSERS           = 0x87u;
const tg_nid_type ST_SOFTFILES          = 0x88u;
const tg_nid_type ST_HARDFILES          = 0x89u;
        // Unused (0x8A-0x8F)
const tg_nid_type ST_LASTPING           = 0x90u; // <uint32>
const tg_nid_type ST_VERSION            = 0x91u; // <string>
const tg_nid_type ST_UDPFLAGS           = 0x92u; // <uint32>
const tg_nid_type ST_AUXPORTSLIST       = 0x93u; // <string>
const tg_nid_type ST_LOWIDUSERS         = 0x94u; // <uint32>
const tg_nid_type ST_UDPKEY             = 0x95u; // <uint32>
const tg_nid_type ST_UDPKEYIP           = 0x96u; // <uint32>
const tg_nid_type ST_TCPPORTOBFUSCATION = 0x97u; // <uint16>
const tg_nid_type ST_UDPPORTOBFUSCATION = 0x98u; // <uint16>

// client tags
const tg_nid_type CT_NAME                         = 0x01u;
const tg_nid_type CT_SERVER_UDPSEARCH_FLAGS       = 0x0Eu;
const tg_nid_type CT_PORT                         = 0x0Fu;
const tg_nid_type CT_VERSION                      = 0x11u;
const tg_nid_type CT_SERVER_FLAGS                 = 0x20u; // currently only used to inform a server about supported features
const tg_nid_type CT_EMULECOMPAT_OPTIONS          = 0xEFu;
const tg_nid_type CT_EMULE_RESERVED1              = 0xF0u;
const tg_nid_type CT_EMULE_RESERVED2              = 0xF1u;
const tg_nid_type CT_EMULE_RESERVED3              = 0xF2u;
const tg_nid_type CT_EMULE_RESERVED4              = 0xF3u;
const tg_nid_type CT_EMULE_RESERVED5              = 0xF4u;
const tg_nid_type CT_EMULE_RESERVED6              = 0xF5u;
const tg_nid_type CT_EMULE_RESERVED7              = 0xF6u;
const tg_nid_type CT_EMULE_RESERVED8              = 0xF7u;
const tg_nid_type CT_EMULE_RESERVED9              = 0xF8u;
const tg_nid_type CT_EMULE_UDPPORTS               = 0xF9u;
const tg_nid_type CT_EMULE_MISCOPTIONS1           = 0xFAu;
const tg_nid_type CT_EMULE_VERSION                = 0xFBu;
const tg_nid_type CT_EMULE_BUDDYIP                = 0xFCu;
const tg_nid_type CT_EMULE_BUDDYUDP               = 0xFDu;
const tg_nid_type CT_EMULE_MISCOPTIONS2           = 0xFEu;
const tg_nid_type CT_EMULE_RESERVED13             = 0xFFu;

// old emule flags
const unsigned int ET_COMPRESSION          = 0x20u;
const unsigned int ET_UDPPORT              = 0x21u;
const unsigned int ET_UDPVER               = 0x22u;
const unsigned int ET_SOURCEEXCHANGE       = 0x23u;
const unsigned int ET_COMMENTS             = 0x24u;
const unsigned int ET_EXTENDEDREQUEST      = 0x25u;
const unsigned int ET_COMPATIBLECLIENT     = 0x26u;
const unsigned int ET_FEATURES             = 0x27u;        //! bit 0: SecIdent v1 - bit 1: SecIdent v2
const unsigned int ET_MOD_VERSION          = 0x55u;
// ET_FEATURESET        = 0x54u,        // int - [Bloodymad Featureset] // UNUSED
const unsigned int ET_OS_INFO              = 0x94u;         // Reused rand tag (MOD_OXY), because the type is unknown


// capabilities
const boost::uint32_t SRVCAP_ZLIB               = 0x0001;
const boost::uint32_t SRVCAP_IP_IN_LOGIN        = 0x0002;
const boost::uint32_t SRVCAP_AUXPORT            = 0x0004;
const boost::uint32_t SRVCAP_NEWTAGS            = 0x0008;
const boost::uint32_t SRVCAP_UNICODE            = 0x0010;
const boost::uint32_t SRVCAP_LARGEFILES         = 0x0100;
const boost::uint32_t SRVCAP_SUPPORTCRYPT       = 0x0200;
const boost::uint32_t SRVCAP_REQUESTCRYPT       = 0x0400;
const boost::uint32_t SRVCAP_REQUIRECRYPT       = 0x0800;

const boost::uint32_t CAPABLE_ZLIB              = SRVCAP_ZLIB;
const boost::uint32_t CAPABLE_IP_IN_LOGIN_FRAME = SRVCAP_IP_IN_LOGIN;
const boost::uint32_t CAPABLE_AUXPORT           = SRVCAP_AUXPORT;
const boost::uint32_t CAPABLE_NEWTAGS           = SRVCAP_NEWTAGS;
const boost::uint32_t CAPABLE_UNICODE           = SRVCAP_UNICODE;
const boost::uint32_t CAPABLE_LARGEFILES        = SRVCAP_LARGEFILES;

enum tg_types
{
    TAGTYPE_UNDEFINED   = 0x00, // special tag definition for empty objects
    TAGTYPE_HASH16      = 0x01,
    TAGTYPE_STRING      = 0x02,
    TAGTYPE_UINT32      = 0x03,
    TAGTYPE_FLOAT32     = 0x04,
    TAGTYPE_BOOL        = 0x05,
    TAGTYPE_BOOLARRAY   = 0x06,
    TAGTYPE_BLOB        = 0x07,
    TAGTYPE_UINT16      = 0x08,
    TAGTYPE_UINT8       = 0x09,
    TAGTYPE_BSOB        = 0x0A,
    TAGTYPE_UINT64      = 0x0B,

    // Compressed string types
    TAGTYPE_STR1        = 0x11,
    TAGTYPE_STR2,
    TAGTYPE_STR3,
    TAGTYPE_STR4,
    TAGTYPE_STR5,
    TAGTYPE_STR6,
    TAGTYPE_STR7,
    TAGTYPE_STR8,
    TAGTYPE_STR9,
    TAGTYPE_STR10,
    TAGTYPE_STR11,
    TAGTYPE_STR12,
    TAGTYPE_STR13,
    TAGTYPE_STR14,
    TAGTYPE_STR15,
    TAGTYPE_STR16,
    TAGTYPE_STR17,  // accepted by eMule 0.42f (02-Mai-2004) in receiving code
                    // only because of a flaw, those tags are handled correctly,
                    // but should not be handled at all
    TAGTYPE_STR18,  // accepted by eMule 0.42f (02-Mai-2004) in receiving code
            //  only because of a flaw, those tags are handled correctly,
            // but should not be handled at all
    TAGTYPE_STR19,  // accepted by eMule 0.42f (02-Mai-2004) in receiving code
            // only because of a flaw, those tags are handled correctly,
            // but should not be handled at all
    TAGTYPE_STR20,  // accepted by eMule 0.42f (02-Mai-2004) in receiving code
            // only because of a flaw, those tags are handled correctly,
            // but should not be handled at all
    TAGTYPE_STR21,  // accepted by eMule 0.42f (02-Mai-2004) in receiving code
            // only because of a flaw, those tags are handled correctly,
            // but should not be handled at all
    TAGTYPE_STR22   // accepted by eMule 0.42f (02-Mai-2004) in receiving code
            // only because of a flaw, those tags are handled correctly,
            // but should not be handled at all
};

const char* tagTypetoString(tg_type ttp);

template<typename T> struct tag_type_number;

template<> struct tag_type_number<boost::uint8_t>   { static const tg_type value = TAGTYPE_UINT8;   };
template<> struct tag_type_number<boost::uint16_t>  { static const tg_type value = TAGTYPE_UINT16;  };
template<> struct tag_type_number<boost::uint32_t>  { static const tg_type value = TAGTYPE_UINT32;  };
template<> struct tag_type_number<boost::uint64_t>  { static const tg_type value = TAGTYPE_UINT64;  };
template<> struct tag_type_number<float>            { static const tg_type value = TAGTYPE_FLOAT32; };
template<> struct tag_type_number<bool>             { static const tg_type value = TAGTYPE_BOOL;    };
template<> struct tag_type_number<md4_hash>         { static const tg_type value = TAGTYPE_HASH16;  };

template<typename T> struct tag_uniform_type_number;

template<> struct tag_uniform_type_number<boost::uint8_t>   { static const tg_type value = TAGTYPE_UINT64;  };
template<> struct tag_uniform_type_number<boost::uint16_t>  { static const tg_type value = TAGTYPE_UINT64;  };
template<> struct tag_uniform_type_number<boost::uint32_t>  { static const tg_type value = TAGTYPE_UINT64;  };
template<> struct tag_uniform_type_number<boost::uint64_t>  { static const tg_type value = TAGTYPE_UINT64;  };
template<> struct tag_uniform_type_number<float>            { static const tg_type value = TAGTYPE_FLOAT32; };
template<> struct tag_uniform_type_number<bool>             { static const tg_type value = TAGTYPE_BOOL;    };
template<> struct tag_uniform_type_number<md4_hash>         { static const tg_type value = TAGTYPE_HASH16;  };


template<typename size_type>
class tag_list;

/**
  * this is abstract base tag
 */
class base_tag
{
public:
    friend class libed2k::archive::access;
    template<typename size_type>
    friend class tag_list;

    /**
      * create base tag
      * @param nNameId - one byte tag name
      * @param bNewED2K - flag for control save procedure, when true we use optimal saving
      * for empty tags flag will set automatically after load
      * by default we think packet is bNewED2K, but it has no matter because load is universal procedure
     */
    base_tag(tg_nid_type nNameId, bool bNewED2K = true) : m_strName(""), m_nNameId(nNameId), m_bNewED2K(bNewED2K){}

    /**
      * create base tag with string name
      * @param strName - string tag name
      * @param bNewED2K - flag control saving procedure
      * for empty tags flag will set automatically after load
      * by default we think packet is bNewED2K, but it has no matter because load is universal procedure
      *
     */
    base_tag(const std::string& strName, bool bNewED2K = true) : m_strName(strName), m_nNameId(0), m_bNewED2K(bNewED2K){}

    virtual ~base_tag()
    {
    }

    bool defined() const
    {
        return (!m_strName.empty() || (m_nNameId != 0));
    }

    bool isNewED2K() const
    {
        return (m_bNewED2K);
    }

    const std::string getName() const;
    tg_nid_type getNameId() const;

    /**
      * get real type of tag for new format dependents on size
     */
    virtual tg_type getType() const = 0;

    /**
      * get uniform type of tag independent on size
     */
    virtual tg_type getUniformType() const = 0;
    virtual void load(archive::ed2k_iarchive& ar);
    virtual void save(archive::ed2k_oarchive& ar);

    virtual bool is_equal(const base_tag* pt) const;
    virtual void dump() const;

    /**
      * values extractors
     */
    virtual boost::uint64_t     asInt() const;
    virtual const std::string&  asString() const;
    virtual bool                asBool() const;
    virtual float               asFloat() const;
    virtual const std::vector<char>&    asBlob() const;
    virtual const md4_hash&     asHash() const;

    bool operator==(const std::string& strName) const
    {
        return (!m_strName.empty() && m_strName == strName);
    }

    bool operator==(tg_nid_type nId) const
    {
        return (m_nNameId != TAGTYPE_UNDEFINED && m_nNameId == nId);
    }

    LIBED2K_SERIALIZATION_SPLIT_MEMBER()
protected:
    base_tag(const std::string& strName, tg_nid_type nNameId);
private:
    std::string     m_strName;
    tg_nid_type     m_nNameId;
    bool            m_bNewED2K;
};

/**
  * fixed length types
 */
template<typename T>
class typed_tag : public base_tag
{
public:
    friend class libed2k::archive::access;
    template<typename size_type>
    friend class tag_list;

    /**
      * for empty tag we don't to set special protocol level
     */
    typed_tag(tg_nid_type nNameId) : base_tag(nNameId){}
    typed_tag(const std::string& strName) : base_tag(strName){}

    /**
      * for tag with data we have to set correct protocol level
     */
    typed_tag(T t, tg_nid_type nNameId, bool bNewED2K) : base_tag(nNameId, bNewED2K), m_value(t){}
    typed_tag(T t, const std::string& strName, bool bNewED2K) : base_tag(strName, bNewED2K), m_value(t){}
    virtual ~typed_tag() {}


    virtual tg_type getType() const
    {
        return tag_type_number<T>::value;
    }

    virtual tg_type getUniformType() const
    {
        return tag_uniform_type_number<T>::value;
    }

    // can't call one serialize because base class is split
    void load(archive::ed2k_iarchive& ar)
    {
        ar & m_value;
    }

    void save(archive::ed2k_oarchive& ar)
    {
        base_tag::save(ar);
        ar & m_value;
    }

    virtual bool is_equal(const base_tag* pt) const
    {
        if (base_tag::is_equal(pt))
        {
            return (m_value == (reinterpret_cast<const typed_tag<T>* >(pt))->m_value);
        }

        return (false);
    }

    virtual boost::uint64_t asInt() const
    {
        return (base_tag::asInt());
    }

    virtual bool asBool() const
    {
        return (base_tag::asBool());
    }

    virtual float asFloat() const
    {
        return (base_tag::asFloat());
    }

    virtual const md4_hash& asHash() const
    {
        return (base_tag::asHash());
    }

    operator T() const
    {
        return (m_value);
    }

    LIBED2K_SERIALIZATION_SPLIT_MEMBER()
protected:
    typed_tag(const std::string& strName,  tg_nid_type nNameId) : base_tag(strName, nNameId)
    {
    }
private:
    T   m_value;
};

template<>
inline boost::uint64_t typed_tag<boost::uint64_t>::asInt() const
{
    return (m_value);
}

template<>
inline boost::uint64_t typed_tag<boost::uint32_t>::asInt() const
{
    return (static_cast<boost::uint32_t>(m_value));
}

template<>
inline boost::uint64_t typed_tag<boost::uint16_t>::asInt() const
{
    return (static_cast<boost::uint16_t>(m_value));
}

template<>
inline boost::uint64_t typed_tag<boost::uint8_t>::asInt() const
{
    return (static_cast<boost::uint8_t>(m_value));
}

template<>
inline bool typed_tag<bool>::asBool() const
{
    return (m_value);
}

template<>
inline float typed_tag<float>::asFloat() const
{
    return (m_value);
}

template<>
inline const md4_hash& typed_tag<md4_hash>::asHash() const
{
    return (m_value);
}

/**
  * all string data handler
  *
 */
class string_tag : public base_tag
{
public:
    friend class libed2k::archive::access;
    template<typename size_type>
    friend class tag_list;

    /**
      * for empty tags we don't set special protocol level
     */
    string_tag(tg_types type, tg_nid_type nNameId) : base_tag(nNameId), m_type(type){}
    string_tag(tg_types type, const std::string& strName) : base_tag(strName), m_type(type){ }

    /**
      * for tags with data we have to set correct protocol level
      * type auto setting executed for new ED2K
     */
    string_tag(const std::string& strValue, tg_nid_type nNameId, bool bNewED2K) : base_tag(nNameId, bNewED2K)
    {
        m_strValue = strValue;

        // use compression only on new ED2K
        if (isNewED2K())
        {
            length2type();
        }
        else
        {
            m_type = TAGTYPE_STRING;
        }
    }

    /**
      * for tags with data we have to set correct protocol level
      * type auto setting executed for new ED2K
     */
    string_tag(const std::string& strValue, const std::string& strName, bool bNewED2K) : base_tag(strName, bNewED2K)
    {
        m_strValue = strValue;

        // use compression only on new ED2K
        if (isNewED2K())
        {
            length2type();
        }
        else
        {
            m_type = TAGTYPE_STRING;
        }
    }

    /**
      * special case without any changes in types
     */
    string_tag(const std::string& strValue, tg_type type, tg_nid_type nNameId, bool bNewED2K) : base_tag(nNameId, bNewED2K), m_type(type), m_strValue(strValue)
    {
    }

    string_tag(const std::string& strValue, tg_type type, const std::string& strName, bool bNewED2K) : base_tag(strName, bNewED2K), m_type(type), m_strValue(strValue)
    {
    }

    virtual tg_type getType() const;
    virtual tg_type getUniformType() const;

    virtual bool is_equal(const base_tag* pt) const;

    operator std::string() const
    {
        return (m_strValue);
    }

    const std::string& asString() const;

    void save(archive::ed2k_oarchive& ar);
    void load(archive::ed2k_iarchive& ar);
    LIBED2K_SERIALIZATION_SPLIT_MEMBER()
protected:
    string_tag(tg_types type, const std::string& strName, tg_nid_type nNameId);
private:
    tg_type m_type;
    std::string m_strValue;

    /**
      * generate tag type by string length
     */
    void length2type();
};

/**
  * class for handle BLOB data
 */
class array_tag : public base_tag
{
public:
    friend class libed2k::archive::access;
    template<typename size_type>
    friend class tag_list;

    /**
      * for empty tags we don't set protocol level
     */
    array_tag(tg_nid_type nNameId) : base_tag(nNameId) { }
    array_tag(const std::string& strName) : base_tag(strName) {}

    /**
      * for tags with value we have to set special protocol level
     */
    array_tag(const std::vector<char>& vValue, tg_nid_type nNameId, bool bNewED2K) : base_tag(nNameId, bNewED2K), m_value(vValue)
    {
    }

    array_tag(const std::vector<char>& vValue, const std::string& strName, bool bNewED2K) : base_tag(strName, bNewED2K), m_value(vValue)
    {
    }

    virtual tg_type getType() const
    {
       return (TAGTYPE_BLOB);
    }

    virtual tg_type getUniformType() const
    {
        return (TAGTYPE_BLOB);
    }

    virtual bool is_equal(const base_tag* pt) const
    {
        if (base_tag::is_equal(pt))
        {
            return (m_value == (reinterpret_cast<const array_tag*>(pt))->m_value);
        }

        return (false);
    }

    operator std::vector<char>() const
    {
        return (m_value);
    }

    virtual const std::vector<char>& asBlob() const;
    std::vector<char>* asBlob();

    void save(archive::ed2k_oarchive& ar);
    void load(archive::ed2k_iarchive& ar);


    LIBED2K_SERIALIZATION_SPLIT_MEMBER()
protected:
    array_tag(const std::string& strName, tg_nid_type nNameId);

private:
    std::vector<char> m_value;
};

template<class T>
boost::shared_ptr<base_tag> make_typed_tag(T t, tg_nid_type nNameId, bool bNewED2K)
{
	return (boost::shared_ptr<base_tag>(new typed_tag<T>(t, nNameId, bNewED2K)));
}

template<class T>
boost::shared_ptr<base_tag> make_typed_tag(T t, const std::string& strName, bool bNewED2K)
{
	return (boost::shared_ptr<base_tag>(new typed_tag<T>(t, strName, bNewED2K)));
}

boost::shared_ptr<base_tag> make_string_tag(const std::string& strValue, tg_nid_type nNameId, bool bNewED2K);
boost::shared_ptr<base_tag> make_string_tag(const std::string& strValue, const std::string& strName, bool bNewED2K);
boost::shared_ptr<base_tag> make_blob_tag(const std::vector<char>& vValue, tg_nid_type nNameId, bool bNewED2K);
boost::shared_ptr<base_tag> make_blob_tag(const std::vector<char>& vValue, const std::string& strName, bool bNewED2K);

bool is_string_tag(const boost::shared_ptr<base_tag> p);
bool is_int_tag(const boost::shared_ptr<base_tag> p);

/**
  * class for tag list representation
  * used to decode/encode tag list appended sequences in ed2k packets
  * facade on std::deque stl container
 */
template<typename size_type>
class tag_list
{
public:
    typedef boost::shared_ptr<base_tag> value_type;
    typedef std::deque<value_type>::iterator iterator;
    typedef std::deque<value_type>::const_iterator const_iterator;
    template<typename U>
    friend bool operator==(const tag_list<U>& t1, const tag_list<U>& t2);
    template<typename U>
    friend bool operator!=(const tag_list<U>& t1, const tag_list<U>& t2);
    tag_list(){}

    void add_tag(value_type ptag){
        m_container.push_back(ptag);
    }

    void clear() {
        m_container.clear();
    }

    const value_type operator[](size_t n) const{
        LIBED2K_ASSERT(n < m_container.size());
        return (m_container[n]);
    }

    tg_nid_type getTagNameId(size_t n) const{
        LIBED2K_ASSERT(n < m_container.size());
        return m_container[n]->getNameId();
    }

    tg_type     getTagType(size_t n) const{
        LIBED2K_ASSERT(n < m_container.size());
        return m_container[n]->getType();
    }

    const value_type getTagByNameId(tg_nid_type nId) const{
        for (size_t n = 0; n < m_container.size(); n++){
            if (*m_container[n] == nId){
                return (m_container[n]);
            }
        }
        return (value_type());
    }

    const value_type getTagByName(const std::string& strName) const{
        for (size_t n = 0; n < m_container.size(); n++){
            if (*m_container[n] == strName) {
                return (m_container[n]);
            }
        }
        return (boost::shared_ptr<base_tag>());
    }
    
    /**
      * return special tag as string or int
      * if tag not exists or his type is not string returns empty string or zero int
     */
    std::string getStringTagByNameId(tg_nid_type nId) const{
        if (value_type p = getTagByNameId(nId)){
            try{
                return (p->asString());
            }catch(libed2k_exception& e){
                ERR("Incorrect to string conversion: " << e.what());
            }
        }
        return (std::string(""));
    }

    std::string getStringTagByName(const std::string strName) const{
        if (value_type p = getTagByName(strName)){
            try{
                return (p->asString());
            }catch(libed2k_exception& e){
                ERR("Incorrect to string conversion: " << e.what());
            }
        }
        return (std::string(""));
    }

    boost::uint64_t getIntTagByNameId(tg_nid_type nId) const{
        if (value_type p = getTagByNameId(nId)){
            try{
                return (p->asInt());
            }catch(libed2k_exception& e){
                ERR("Incorrect to int conversion: " << e.what());
            }
        }
        return 0;
    }

    boost::uint64_t getIntTagByName(const std::string strName) const{
        if (value_type p = getTagByName(strName)){
            try{
                return (p->asInt());
            }catch(libed2k_exception& e){
                ERR("Incorrect to int conversion: " << e.what());
            }
        }
        return 0;
    }

    void save(archive::ed2k_oarchive& ar);
    void load(archive::ed2k_iarchive& ar);
    void dump() const;

    // STL container like methods
    size_t size() const {
        return (m_container.size());
    }

    const_iterator begin() const { return m_container.begin(); }
    iterator begin(){ return m_container.begin(); }
    const_iterator end() const { return m_container.end(); }
    iterator end(){ return m_container.end(); }
    void push_back(value_type ptag) { add_tag(ptag); }

    LIBED2K_SERIALIZATION_SPLIT_MEMBER()
private:
    std::deque<value_type>   m_container;
};


template<typename size_type>
void tag_list<size_type>::save(archive::ed2k_oarchive& ar)
{
    size_type nSize = static_cast<size_type>(m_container.size());
    ar & nSize;

    for (size_t n = 0; n < m_container.size(); n++)
    {
        ar & *(m_container[n].get());
    }
}

template<typename size_type>
void tag_list<size_type>::load(archive::ed2k_iarchive& ar)
{
    size_type nSize;
    ar & nSize;

    for (size_t n = 0; n < nSize; n++)
    {
        // read tag header
        tg_type nType       = 0;
        tg_nid_type nNameId = 0;
        std::string strName;

        ar & nType;
        if (nType & 0x80)
        {
            nType &= 0x7F;
            ar & nNameId;
        }
        else
        {
            boost::uint16_t nLength;
            ar & nLength;

            if (nLength == 1)
            {
                ar & nNameId;
            }
            else
            {
                strName.resize(nLength);
                ar & strName;
            }
        }

        // don't process bool arrays
        if (nType == TAGTYPE_BOOLARRAY)
        {
            // this tag must been passed
            boost::uint16_t nLength;
            ar & nLength;

#ifdef WIN32
			// windows generates exceptions independent by exceptions flags in stream
			try
			{
                ar.container().seekg((nLength/8) + 1, std::ios::cur);
			}
			catch(std::ios_base::failure&)
			{
				throw libed2k::libed2k_exception(libed2k::errors::unexpected_istream_error);
			}
#else
			ar.container().seekg((nLength/8) + 1, std::ios::cur);
#endif


            // check status
            if (!ar.container().good())
            {
                throw libed2k::libed2k_exception(libed2k::errors::unexpected_istream_error);
            }

            continue;
        }

        switch (nType)
        {
        case TAGTYPE_UINT64:
                m_container.push_back(value_type(new typed_tag<boost::uint64_t>(strName, nNameId)));
                break;
        case TAGTYPE_UINT32:
                m_container.push_back(value_type(new typed_tag<boost::uint32_t>(strName, nNameId)));
                break;
        case TAGTYPE_UINT16:
                m_container.push_back(value_type(new typed_tag<boost::uint16_t>(strName, nNameId)));
                break;
        case TAGTYPE_UINT8:
                m_container.push_back(value_type(new typed_tag<boost::uint8_t>(strName, nNameId)));
                break;
        case TAGTYPE_FLOAT32:
                m_container.push_back(value_type(new typed_tag<float>(strName, nNameId)));
                break;
        case TAGTYPE_BOOL:
                m_container.push_back(value_type(new typed_tag<bool>(strName, nNameId)));
                break;
        case TAGTYPE_HASH16:
                m_container.push_back(value_type(new typed_tag<md4_hash>(strName, nNameId)));
                break;
        case TAGTYPE_BLOB:
                m_container.push_back(value_type(new array_tag(strName, nNameId)));
                break;
        case TAGTYPE_STRING:
        case TAGTYPE_STR1:
        case TAGTYPE_STR2:
        case TAGTYPE_STR3:
        case TAGTYPE_STR4:
        case TAGTYPE_STR5:
        case TAGTYPE_STR6:
        case TAGTYPE_STR7:
        case TAGTYPE_STR8:
        case TAGTYPE_STR9:
        case TAGTYPE_STR10:
        case TAGTYPE_STR11:
        case TAGTYPE_STR12:
        case TAGTYPE_STR13:
        case TAGTYPE_STR14:
        case TAGTYPE_STR15:
        case TAGTYPE_STR16:
                m_container.push_back(value_type(new string_tag(static_cast<tg_types>(nType), strName, nNameId)));
                break;
        default:
            throw libed2k_exception(errors::invalid_tag_type);
            break;
        };

        ar & *m_container.back();
    }
}

template<typename size_type>
void tag_list<size_type>::dump() const
{
    DBG("size type is: " << sizeof(size_type));
    DBG("count: " << m_container.size());
    std::for_each(m_container.begin(), m_container.end(), boost::mem_fn(&base_tag::dump));
}

template<typename size_type>
bool operator==(const tag_list<size_type>& t1, const tag_list<size_type>& t2){
    if (t1.size() != t2.size()) return (false);

    for(size_t n = 0; n < t1.size(); ++n)
    {
        bool found = false;
        //TODO - replace it after remove shared_ptr
        boost::shared_ptr<base_tag> ptr = t1[n]; // temporary hack

        for (size_t m = 0; m < t2.size(); ++m){
            if (t2.m_container[m]->is_equal(ptr.get())){
                found = true;
                break;
            }
        }

        if (!found){
            return (false);
        }
    }

    return (true);
}

template<typename size_type>
bool operator!=(const tag_list<size_type>& t1, const tag_list<size_type>& t2){ return (!(t1 == t2));}

}

#endif // __CTAG__
