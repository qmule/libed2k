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

class tag_list;

/**
  * this is abstract base tag
 */
class base_tag
{
public:
    friend class libed2k::archive::access;
    friend class tag_list;

    base_tag(tg_nid_type nNameId) : m_strName(""), m_nNameId(nNameId){}
    base_tag(const std::string& strName) : m_strName(strName){}

    virtual ~base_tag()
    {
    }

    bool defined() const
    {
        return (!m_strName.empty() || (m_nNameId != 0));
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
};

/**
  * fixed length types
 */
template<typename T>
class typed_tag : public base_tag
{
public:
    friend class libed2k::archive::access;
    friend class tag_list;

    typed_tag(boost::uint8_t nNameId) : base_tag(nNameId){}
    typed_tag(const std::string& strName) : base_tag(strName){}
    typed_tag(T t, boost::uint8_t nNameId) : base_tag(nNameId), m_value(t){}
    typed_tag(T t, const std::string& strName) : base_tag(strName), m_value(t){}
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

    LIBED2K_SERIALIZATION_SPLIT_MEMBER()
protected:
    typed_tag(const std::string& strName, boost::uint8_t nNameId) : base_tag(strName, nNameId)
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
    friend class tag_list;

    string_tag(tg_types type, tg_nid_type nNameId) : base_tag(nNameId), m_type(type)
    {
    }

    string_tag(tg_types type, const std::string& strName) : base_tag(strName), m_type(type)
    {
    }

    string_tag(const std::string& strValue, tg_nid_type nNameId) : base_tag(nNameId)
    {
        m_strValue = strValue;
        length2type();
    }

    string_tag(const std::string& strValue, const std::string& strName) : base_tag(strName)
    {
        m_strValue = strValue;
        length2type();
    }

    string_tag(const std::string& strValue, tg_type type, tg_nid_type nNameId) : base_tag(nNameId), m_type(type), m_strValue(strValue)
    {
    }

    string_tag(const std::string& strValue, tg_type type, const std::string& strName) : base_tag(strName), m_type(type), m_strValue(strValue)
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
    friend class tag_list;

    array_tag(tg_nid_type nNameId) : base_tag(nNameId)
    {
    }

    array_tag(const std::string& strName) : base_tag(strName)
    {
    }

    array_tag(const std::vector<boost::uint8_t>& vValue, tg_nid_type nNameId) : base_tag(nNameId), m_value(vValue)
    {
    }

    array_tag(const std::vector<boost::uint8_t>& vValue, const std::string& strName) : base_tag(strName), m_value(vValue)
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

    void save(archive::ed2k_oarchive& ar);
    void load(archive::ed2k_iarchive& ar);

    LIBED2K_SERIALIZATION_SPLIT_MEMBER()
protected:
    array_tag(const std::string& strName, tg_nid_type nNameId);

private:
    std::vector<boost::uint8_t> m_value;
    tg_type m_type;
};

/**
  * class for tag list representation
  * used to decode/encode tag list appended sequnces
 */
class tag_list
{
public:
    friend bool operator==(const tag_list& t1, const tag_list& t2);
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

}

#endif // __CTAG__
