#include "ctag.hpp"

namespace libed2k{

const char* tagTypetoString(tg_type ttp)
{
    static const char* chUnknown = "unknown result";

    static const char* chFormat[] =
    {
        "TAGTYPE_UNDEFINED",
        "TAGTYPE_HASH16",
        "TAGTYPE_STRING",
        "TAGTYPE_UINT32",
        "TAGTYPE_FLOAT32",
        "TAGTYPE_BOOL",
        "TAGTYPE_BOOLARRAY",
        "TAGTYPE_BLOB",
        "TAGTYPE_UINT16",
        "TAGTYPE_UINT8",
        "TAGTYPE_BSOB",
        "TAGTYPE_UINT64",// 0x0B,
        "TAGTYPE_HOLE",  // 0x0C
        "TAGTYPE_HOLE",  // 0x0D
        "TAGTYPE_HOLE",  // 0x0E
        "TAGTYPE_HOLE",  // 0x0F
        "TAGTYPE_HOLE",  // 0x10
        // Compressed string types
        "TAGTYPE_STR1",  // = 0x11,
        "TAGTYPE_STR2",
        "TAGTYPE_STR3",
        "TAGTYPE_STR4",
        "TAGTYPE_STR5",
        "TAGTYPE_STR6",
        "TAGTYPE_STR7",
        "TAGTYPE_STR8",
        "TAGTYPE_STR9",
        "TAGTYPE_STR10",
        "TAGTYPE_STR11",
        "TAGTYPE_STR12",
        "TAGTYPE_STR13",
        "TAGTYPE_STR14",
        "TAGTYPE_STR15",
        "TAGTYPE_STR16",
        "TAGTYPE_STR17",
        "TAGTYPE_STR18",
        "TAGTYPE_STR19",
        "TAGTYPE_STR20",
        "TAGTYPE_STR21",
        "TAGTYPE_STR22"
    };

    if (ttp < sizeof(chFormat)/sizeof(chFormat[0]))
    {
        return chFormat[ttp];
    }

    return (chUnknown);
}

// base tag
base_tag::base_tag(const std::string& strName, tg_nid_type nNameId) : m_strName(strName), m_nNameId(nNameId)
{
}

void base_tag::load(archive::ed2k_iarchive& ar)
{
    // do nothing - all will be done in tag list
}

void base_tag::save(archive::ed2k_oarchive& ar)
{
    // save tag header
    tg_nid_type nType = getType();

    if (m_strName.empty())
    {
        if (m_bNewED2K)
        {
            // for new we write special tag flag
            nType |= 0x80;
            ar & nType;
        }
        else
        {
            // for old we write same type and name with length 1
            boost::uint16_t nLength = 1;
            ar & nType;
            ar & nLength;
        }

        ar & m_nNameId;
    }
    else
    {
        boost::uint16_t nLength = static_cast<boost::uint16_t>(m_strName.size());
        ar & nType;
        ar & nLength;
        ar & m_strName;
    }
}

const std::string base_tag::getName() const
{
    return (m_strName);
}

tg_nid_type base_tag::getNameId() const
{
    return (m_nNameId);
}

bool base_tag::is_equal(const base_tag* pt) const
{
    return (pt->getType() == getType());
}

void base_tag::dump() const
{
    DBG("base_tag::dump");
    DBG("type: " << tagTypetoString(getType()));
    DBG("name: " << m_strName.c_str());
}

boost::uint64_t base_tag::asInt() const
{
    throw libed2k_exception(errors::incompatible_tag_getter);
}

const std::string& base_tag::asString() const
{
    throw libed2k_exception(errors::incompatible_tag_getter);
}

bool base_tag::asBool() const
{
    throw libed2k_exception(errors::incompatible_tag_getter);
}

float base_tag::asFloat() const
{
    throw libed2k_exception(errors::incompatible_tag_getter);
}

const blob_type& base_tag::asBlob() const
{
    throw libed2k_exception(errors::incompatible_tag_getter);
}

template<>
boost::uint64_t typed_tag<boost::uint64_t>::asInt() const
{
    return (m_value);
}

template<>
boost::uint64_t typed_tag<boost::uint32_t>::asInt() const
{
    return (static_cast<boost::uint32_t>(m_value));
}

template<>
boost::uint64_t typed_tag<boost::uint16_t>::asInt() const
{
    return (static_cast<boost::uint16_t>(m_value));
}

template<>
boost::uint64_t typed_tag<boost::uint8_t>::asInt() const
{
    return (static_cast<boost::uint8_t>(m_value));
}

template<>
bool typed_tag<bool>::asBool() const
{
    return (m_value);
}

template<>
float typed_tag<float>::asFloat() const
{
    return (m_value);
}

string_tag::string_tag(tg_types type, const std::string& strName, tg_nid_type nNameId) : base_tag(strName, nNameId), m_type(static_cast<tg_type>(type))
{

}

void string_tag::save(archive::ed2k_oarchive& ar)
{
    base_tag::save(ar);

    if (m_type == TAGTYPE_STRING)
    {
        boost::uint16_t nLength = static_cast<boost::uint16_t>(m_strValue.size());
        ar & nLength;
    }

    ar & m_strValue;
}

void string_tag::load(archive::ed2k_iarchive& ar)
{
    boost::uint16_t nLength;

    if (m_type >= TAGTYPE_STR1 && m_type <= TAGTYPE_STR16)
    {
        nLength = static_cast<boost::uint16_t>(m_type - TAGTYPE_STR1);
        ++nLength;
    }
    else
    {
        ar & nLength;
    }

    m_strValue.resize(nLength);
    ar & m_strValue;
}

void string_tag::length2type()
{
    if (m_strValue.size() >= 1 && m_strValue.size() <= 16)
    {
        m_type = static_cast<tg_nid_type>(TAGTYPE_STR1 + m_strValue.size());
        --m_type;
    }
    else
    {
        m_type = TAGTYPE_STRING;
    }
}

tg_type string_tag::getType() const
{
    return (m_type);
}

bool string_tag::is_equal(const base_tag* pt) const
{
    if (base_tag::is_equal(pt))
    {
        return (m_strValue == (reinterpret_cast<const string_tag*>(pt))->m_strValue);
    }

    return (false);
}

void string_tag::dump() const
{
    base_tag::dump();
    DBG("value: " << m_strValue.c_str());
}

const std::string& string_tag::asString() const
{
    return (m_strValue);
}

array_tag::array_tag(const std::string& strName, tg_nid_type nNameId) : base_tag(strName, nNameId)
{
}

const blob_type& array_tag::asBlob() const
{
    return (m_value);
}

void array_tag::save(archive::ed2k_oarchive& ar)
{
    base_tag::save(ar);
    boost::uint32_t nSize = m_value.size();
    ar & nSize;
    ar.raw_write(reinterpret_cast<const char*>(&m_value[0]), m_value.size());
}

void array_tag::load(archive::ed2k_iarchive& ar)
{
    boost::uint32_t nSize;
    ar & nSize;

    if (nSize > 0)
    {
        // avoid huge memory allocation on incorrect tags
        if (nSize > 65535)
        {
            int nSignLength = -1*nSize;

			// windows throw exceptions always
#ifdef WIN32
			try
			{
                ar.container().seekg(nSize, std::ios::cur);   // go to end of blob
			}
			catch(std::ios_base::failure& ex)
			{
				throw libed2k::libed2k_exception(libed2k::errors::blob_tag_too_long);
			}
#else
			ar.container().seekg(nSize, std::ios::cur);   // go to end of blob
#endif


            if (!ar.container().good())
            {
                throw libed2k::libed2k_exception(libed2k::errors::blob_tag_too_long);
            }

            ar.container().seekg(nSignLength, std::ios::cur);   // go back
        }

        m_value.resize(nSize);
        ar.raw_read(reinterpret_cast<char*>(&m_value[0]), m_value.size());
    }
}

boost::shared_ptr<base_tag> make_string_tag(const std::string& strValue, tg_nid_type nNameId, bool bNewED2K)
{
	return (boost::shared_ptr<base_tag>(new string_tag(strValue, nNameId, bNewED2K)));
}

boost::shared_ptr<base_tag> make_string_tag(const std::string& strValue, const std::string& strName, bool bNewED2K)
{
	return (boost::shared_ptr<base_tag>(new string_tag(strValue, strName, bNewED2K)));
}

boost::shared_ptr<base_tag> make_blob_tag(const blob_type& vValue, tg_nid_type nNameId, bool bNewED2K)
{
	return (boost::shared_ptr<base_tag>(new array_tag(vValue, nNameId, bNewED2K)));
}

boost::shared_ptr<base_tag> make_blob_tag(const blob_type& vValue, const std::string& strName, bool bNewED2K)
{
	return (boost::shared_ptr<base_tag>(new array_tag(vValue, strName, bNewED2K)));
}

/*
std::string asString(const base_tag* ptag)
{
    if (ptag->getType() == TAGTYPE_STRING || (ptag->getType >= TAGTYPE_STR1 && ptag->getType <= TAGTYPE_STR16))
    {
        return *(static_cast<string_tag*>(ptag));
    }
}
*/

}
