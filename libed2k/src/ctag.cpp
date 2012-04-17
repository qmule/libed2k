#include "ctag.hpp"

namespace libed2k{

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

array_tag::array_tag(const std::string& strName, tg_nid_type nNameId) : base_tag(strName, nNameId)
{
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


}
