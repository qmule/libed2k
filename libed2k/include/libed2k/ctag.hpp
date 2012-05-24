#ifndef __CTAG__
#define __CTAG__

#include <iostream>
#include <string>
#include <vector>
#include <cassert>

#include <boost/cstdint.hpp>
#include <boost/mem_fn.hpp>
#include "libed2k/log.hpp"
#include "libed2k/archive.hpp"
#include "libed2k/md4_hash.hpp"

namespace libed2k{

#define CHECK_TAG_TYPE(x)\
if (!x)\
{\
  throw libed2k::libed2k_exception(libed2k::errors::incompatible_tag_getter);\
}

typedef boost::uint8_t tg_nid_type;
typedef boost::uint8_t tg_type;
typedef std::vector<boost::uint8_t> blob_type;

const tg_nid_type FT_FILENAME           = '\x01';    // <string>
const tg_nid_type FT_FILESIZE           = '\x02';    // <uint32>
const tg_nid_type FT_FILESIZE_HI        = '\x3A';    // <uint32>
const tg_nid_type FT_FILETYPE           = '\x03';    // <string> or <uint32>
const tg_nid_type FT_FILEFORMAT         = '\x04';    // <string>
const tg_nid_type FT_LASTSEENCOMPLETE   = '\x05';    // <uint32>
const tg_nid_type FT_TRANSFERRED        = '\x08';    // <uint32>
const tg_nid_type FT_GAPSTART           = '\x09';    // <uint32>
const tg_nid_type FT_GAPEND             = '\x0A';    // <uint32>
const tg_nid_type FT_PARTFILENAME       = '\x12';    // <string>
const tg_nid_type FT_OLDDLPRIORITY      = '\x13';    // Not used anymore
const tg_nid_type FT_STATUS             = '\x14';    // <uint32>
const tg_nid_type FT_SOURCES            = '\x15';    // <uint32>
const tg_nid_type FT_PERMISSIONS        = '\x16';    // <uint32>
const tg_nid_type FT_OLDULPRIORITY      = '\x17';    // Not used anymore
const tg_nid_type FT_DLPRIORITY         = '\x18';    // Was 13
const tg_nid_type FT_ULPRIORITY         = '\x19';    // Was 17
const tg_nid_type FT_KADLASTPUBLISHKEY  = '\x20';    // <uint32>
const tg_nid_type FT_KADLASTPUBLISHSRC  = '\x21';    // <uint32>
const tg_nid_type FT_FLAGS              = '\x22';    // <uint32>
const tg_nid_type FT_DL_ACTIVE_TIME     = '\x23';    // <uint32>
const tg_nid_type FT_CORRUPTEDPARTS     = '\x24';    // <string>
const tg_nid_type FT_DL_PREVIEW         = '\x25';
const tg_nid_type FT_KADLASTPUBLISHNOTES= '\x26';    // <uint32>
const tg_nid_type FT_AICH_HASH          = '\x27';
const tg_nid_type FT_COMPLETE_SOURCES   = '\x30';    // nr. of sources which share a


// complete version of the
// associated file (supported
// by eserver 16.46+) statistic

const tg_nid_type FT_PUBLISHINFO        = '\x33';    // <uint32>
const tg_nid_type FT_ATTRANSFERRED      = '\x50';    // <uint32>
const tg_nid_type FT_ATREQUESTED        = '\x51';    // <uint32>
const tg_nid_type FT_ATACCEPTED         = '\x52';    // <uint32>
const tg_nid_type FT_CATEGORY           = '\x53';    // <uint32>
const tg_nid_type FT_ATTRANSFERREDHI    = '\x54';    // <uint32>
const tg_nid_type FT_MEDIA_ARTIST       = '\xD0';    // <string>
const tg_nid_type FT_MEDIA_ALBUM        = '\xD1';    // <string>
const tg_nid_type FT_MEDIA_TITLE        = '\xD2';    // <string>
const tg_nid_type FT_MEDIA_LENGTH       = '\xD3';    // <uint32> !!!
const tg_nid_type FT_MEDIA_BITRATE      = '\xD4';    // <uint32>
const tg_nid_type FT_MEDIA_CODEC        = '\xD5';    // <string>
const tg_nid_type FT_FILERATING         = '\xF7';    // <uint8>


// server tags
const tg_nid_type ST_SERVERNAME         = '\x01'; // <string>
// Unused (0x02-0x0A)
const tg_nid_type ST_DESCRIPTION        = '\x0B'; // <string>
const tg_nid_type ST_PING               = '\x0C'; // <uint32>
const tg_nid_type ST_FAIL               = '\x0D'; // <uint32>
const tg_nid_type ST_PREFERENCE         = '\x0E'; // <uint32>
        // Unused (0x0F-0x84)
const tg_nid_type ST_DYNIP              = '\x85';
const tg_nid_type ST_LASTPING_DEPRECATED= '\x86'; // <uint32> // DEPRECATED, use 0x90
const tg_nid_type ST_MAXUSERS           = '\x87';
const tg_nid_type ST_SOFTFILES          = '\x88';
const tg_nid_type ST_HARDFILES          = '\x89';
        // Unused (0x8A-0x8F)
const tg_nid_type ST_LASTPING           = '\x90'; // <uint32>
const tg_nid_type ST_VERSION            = '\x91'; // <string>
const tg_nid_type ST_UDPFLAGS           = '\x92'; // <uint32>
const tg_nid_type ST_AUXPORTSLIST       = '\x93'; // <string>
const tg_nid_type ST_LOWIDUSERS         = '\x94'; // <uint32>
const tg_nid_type ST_UDPKEY             = '\x95'; // <uint32>
const tg_nid_type ST_UDPKEYIP           = '\x96'; // <uint32>
const tg_nid_type ST_TCPPORTOBFUSCATION = '\x97'; // <uint16>
const tg_nid_type ST_UDPPORTOBFUSCATION = '\x98'; // <uint16>

// client tags
const tg_nid_type CT_NAME                         = '\x01';
const tg_nid_type CT_SERVER_UDPSEARCH_FLAGS       = '\x0E';
const tg_nid_type CT_PORT                         = '\x0F';
const tg_nid_type CT_VERSION                      = '\x11';
const tg_nid_type CT_SERVER_FLAGS                 = '\x20'; // currently only used to inform a server about supported features
const tg_nid_type CT_EMULECOMPAT_OPTIONS          = '\xEF';
const tg_nid_type CT_EMULE_RESERVED1              = '\xF0';
const tg_nid_type CT_EMULE_RESERVED2              = '\xF1';
const tg_nid_type CT_EMULE_RESERVED3              = '\xF2';
const tg_nid_type CT_EMULE_RESERVED4              = '\xF3';
const tg_nid_type CT_EMULE_RESERVED5              = '\xF4';
const tg_nid_type CT_EMULE_RESERVED6              = '\xF5';
const tg_nid_type CT_EMULE_RESERVED7              = '\xF6';
const tg_nid_type CT_EMULE_RESERVED8              = '\xF7';
const tg_nid_type CT_EMULE_RESERVED9              = '\xF8';
const tg_nid_type CT_EMULE_UDPPORTS               = '\xF9';
const tg_nid_type CT_EMULE_MISCOPTIONS1           = '\xFA';
const tg_nid_type CT_EMULE_VERSION                = '\xFB';
const tg_nid_type CT_EMULE_BUDDYIP                = '\xFC';
const tg_nid_type CT_EMULE_BUDDYUDP               = '\xFD';
const tg_nid_type CT_EMULE_MISCOPTIONS2           = '\xFE';
const tg_nid_type CT_EMULE_RESERVED13             = '\xFF';

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
const std::string& tagIdtoString(tg_nid_type tid);

template<typename T> struct tag_type_number;

template<> struct tag_type_number<boost::uint8_t>   { static const tg_type value = TAGTYPE_UINT8;   };
template<> struct tag_type_number<boost::uint16_t>  { static const tg_type value = TAGTYPE_UINT16;  };
template<> struct tag_type_number<boost::uint32_t>  { static const tg_type value = TAGTYPE_UINT32;  };
template<> struct tag_type_number<boost::uint64_t>  { static const tg_type value = TAGTYPE_UINT64;  };
template<> struct tag_type_number<float>            { static const tg_type value = TAGTYPE_FLOAT32; };
template<> struct tag_type_number<bool>             { static const tg_type value = TAGTYPE_BOOL;    };
template<> struct tag_type_number<md4_hash>         { static const tg_type value = TAGTYPE_HASH16;  };

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

    virtual tg_type getType() const = 0;
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
    virtual const blob_type&    asBlob() const;
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
    array_tag(const blob_type& vValue, tg_nid_type nNameId, bool bNewED2K) : base_tag(nNameId, bNewED2K), m_value(vValue)
    {
    }

    array_tag(const blob_type& vValue, const std::string& strName, bool bNewED2K) : base_tag(strName, bNewED2K), m_value(vValue)
    {
    }

    virtual tg_type getType() const
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

    operator blob_type() const
    {
        return (m_value);
    }

    virtual const blob_type& asBlob() const;

    void save(archive::ed2k_oarchive& ar);
    void load(archive::ed2k_iarchive& ar);


    LIBED2K_SERIALIZATION_SPLIT_MEMBER()
protected:
    array_tag(const std::string& strName, tg_nid_type nNameId);

private:
    blob_type m_value;
    tg_type m_type;
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
boost::shared_ptr<base_tag> make_blob_tag(const blob_type& vValue, tg_nid_type nNameId, bool bNewED2K);
boost::shared_ptr<base_tag> make_blob_tag(const blob_type& vValue, const std::string& strName, bool bNewED2K);

bool is_string_tag(const boost::shared_ptr<base_tag> p);
bool is_int_tag(const boost::shared_ptr<base_tag> p);

/**
  * class for tag list representation
  * used to decode/encode tag list appended sequences in ed2k packets
 */
template<typename size_type>
class tag_list
{
public:
    template<typename U>
    friend bool operator==(const tag_list<U>& t1, const tag_list<U>& t2);
    tag_list();
    void add_tag(boost::shared_ptr<base_tag> ptag);
    size_t count() const;
    void clear();
    const boost::shared_ptr<base_tag> operator[](size_t n) const;
    tg_nid_type getTagNameId(size_t n) const;
    tg_type     getTagType(size_t n) const;
    const boost::shared_ptr<base_tag> getTagByNameId(tg_nid_type nId) const;
    const boost::shared_ptr<base_tag> getTagByName(const std::string& strName) const;
    
    /**
      * return special tag as string
      * if tag not exists or his type is not string returns empty string
     */
    std::string getStringTagByNameId(tg_nid_type nId) const;

    void save(archive::ed2k_oarchive& ar);
    void load(archive::ed2k_iarchive& ar);
    void dump() const;
    LIBED2K_SERIALIZATION_SPLIT_MEMBER()
private:
    std::vector<boost::shared_ptr<base_tag> >   m_container;
};

template<typename size_type>
tag_list<size_type>::tag_list()
{

}

template<typename size_type>
size_t tag_list<size_type>::count() const
{
    return (m_container.size());
}

template<typename size_type>
void tag_list<size_type>::clear()
{
    m_container.clear();
}

template<typename size_type>
void tag_list<size_type>::add_tag(boost::shared_ptr<base_tag> ptag)
{
    m_container.push_back(ptag);
}

template<typename size_type>
const boost::shared_ptr<base_tag> tag_list<size_type>::operator[](size_t n) const
{
    BOOST_ASSERT(n < m_container.size());
    return (m_container[n]);
}

template<typename size_type>
tg_nid_type tag_list<size_type>::getTagNameId(size_t n) const
{
    BOOST_ASSERT(n < m_container.size());
    return m_container[n]->getNameId();
}

template<typename size_type>
tg_type tag_list<size_type>::getTagType(size_t n) const
{
    BOOST_ASSERT(n < m_container.size());
    return m_container[n]->getType();
}

template<typename size_type>
const boost::shared_ptr<base_tag> tag_list<size_type>::getTagByNameId(tg_nid_type nId) const
{
    for (size_t n = 0; n < m_container.size(); n++)
    {
        if (*m_container[n] == nId)
        {
            return (m_container[n]);
        }
    }

    return (boost::shared_ptr<base_tag>());
}

template<typename size_type>
const boost::shared_ptr<base_tag> tag_list<size_type>::getTagByName(const std::string& strName) const
{
    for (size_t n = 0; n < m_container.size(); n++)
    {
        if (*m_container[n] == strName)
        {
            return (m_container[n]);
        }
    }

    return (boost::shared_ptr<base_tag>());
}

template<typename size_type>
std::string tag_list<size_type>::getStringTagByNameId(tg_nid_type nId) const
{
    if (boost::shared_ptr<base_tag> p = getTagByNameId(nId))
    {
        try
        {
            return (p->asString());
        }
        catch(libed2k_exception& e)
        {
            ERR("Incorrect type conversion: " << e.what());
        }
    }

    return (std::string(""));
}

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
                m_container.push_back(boost::shared_ptr<base_tag>(new typed_tag<boost::uint64_t>(strName, nNameId)));
                break;
        case TAGTYPE_UINT32:
                m_container.push_back(boost::shared_ptr<base_tag>(new typed_tag<boost::uint32_t>(strName, nNameId)));
                break;
        case TAGTYPE_UINT16:
                m_container.push_back(boost::shared_ptr<base_tag>(new typed_tag<boost::uint16_t>(strName, nNameId)));
                break;
        case TAGTYPE_UINT8:
                m_container.push_back(boost::shared_ptr<base_tag>(new typed_tag<boost::uint8_t>(strName, nNameId)));
                break;
        case TAGTYPE_FLOAT32:
                m_container.push_back(boost::shared_ptr<base_tag>(new typed_tag<float>(strName, nNameId)));
                break;
        case TAGTYPE_BOOL:
                m_container.push_back(boost::shared_ptr<base_tag>(new typed_tag<bool>(strName, nNameId)));
                break;
        case TAGTYPE_HASH16:
                m_container.push_back(boost::shared_ptr<base_tag>(new typed_tag<md4_hash>(strName, nNameId)));
                break;
        case TAGTYPE_BLOB:
                m_container.push_back(boost::shared_ptr<base_tag>(new array_tag(strName, nNameId)));
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
                m_container.push_back(boost::shared_ptr<base_tag>(new string_tag(static_cast<tg_types>(nType), strName, nNameId)));
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
bool operator==(const tag_list<size_type>& t1, const tag_list<size_type>& t2)
{
    // check sizes
    if (t1.count() != t2.count())
    {
        return (false);
    }

    // check elements
    for (size_t n = 0; n < t1.count(); n++)
    {
        if (!t1[n].get()->is_equal(t2[n].get()))
        {
            return (false);
        }
    }

    return (true);
}

}

#endif // __CTAG__
