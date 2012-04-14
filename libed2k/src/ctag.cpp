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
        nType |= 0x80;
        ar & nType;
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
            ar.container().seekg(nSize, std::ios::cur);   // go to end of blob

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

tag_list::tag_list()
{

}

size_t tag_list::count() const
{
    return (m_container.size());
}

void tag_list::clear()
{
    m_container.clear();
}

void tag_list::add_tag(boost::shared_ptr<base_tag> ptag)
{
    m_container.push_back(ptag);
}

const boost::shared_ptr<base_tag> tag_list::operator[](size_t n) const
{
    if (n >= m_container.size())
    {
        throw libed2k_exception(errors::tag_list_index_error);
    }

    return (m_container[n]);
}

void tag_list::save(archive::ed2k_oarchive& ar)
{
    boost::uint16_t nSize = static_cast<boost::uint16_t>(m_container.size());
    ar & nSize;

    for (size_t n = 0; n < m_container.size(); n++)
    {
        ar & *(m_container[n].get());
    }
}

void tag_list::load(archive::ed2k_iarchive& ar)
{
    boost::uint16_t nSize;
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
            ar.container().seekg((nLength/8) + 1, std::ios::cur);

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

bool operator==(const tag_list& t1, const tag_list& t2)
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
