#ifndef __CTAG__
#define __CTAG__

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <boost/cstdint.hpp>
#include "archive.hpp"
#include "md4_hash.hpp"

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

template<typename T>
struct tag_type_number
{
    static const tg_type value = 0;
};

template<> struct tag_type_number<boost::uint8_t> { static const tg_type value = TAGTYPE_UINT8; };
template<> struct tag_type_number<boost::uint16_t> { static const tg_type value = TAGTYPE_UINT16; };
template<> struct tag_type_number<boost::uint32_t> { static const tg_type value = TAGTYPE_UINT32; };
template<> struct tag_type_number<boost::uint64_t> { static const tg_type value = TAGTYPE_UINT64; };
template<> struct tag_type_number<float> { static const tg_type value = TAGTYPE_FLOAT32; };
template<> struct tag_type_number<bool> { static const tg_type value = TAGTYPE_BOOL; };
template<> struct tag_type_number<md4_hash> { static const tg_type value = TAGTYPE_HASH16; };

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

    virtual bool is_equal(const base_tag* pt) const
    {
        return (pt->getType() == getType());
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

    virtual tg_type getType() const
    {
        return (m_type);
    }

    virtual bool is_equal(const base_tag* pt) const
    {
        if (base_tag::is_equal(pt))
        {
            return (m_strValue == (reinterpret_cast<const string_tag*>(pt))->m_strValue);
        }

        return (false);
    }

    operator std::string() const
    {
        return (m_strValue);
    }

    void save(archive::ed2k_oarchive& ar);
    void load(archive::ed2k_iarchive& ar);
    LIBED2K_SERIALIZATION_SPLIT_MEMBER()
protected:
    string_tag(tg_types type, const std::string& strName, tg_nid_type nNameId);
private:
    tg_type m_type;
    std::string m_strValue;
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

    LIBED2K_SERIALIZATION_SPLIT_MEMBER()
    void save(archive::ed2k_oarchive& ar);
    void load(archive::ed2k_iarchive& ar);

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
    if (n >= m_container.size())
    {
        throw libed2k_exception(errors::tag_list_index_error);
    }

    return (m_container[n]);
}

template<typename size_type>
void tag_list<size_type>::save(archive::ed2k_oarchive& ar)
{
    boost::uint16_t nSize = static_cast<boost::uint16_t>(m_container.size());
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
			catch(std::ios_base::failure& f)
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
