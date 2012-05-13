#include "libed2k/ctag.hpp"
#include "libed2k/util.hpp"

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

std::string taggIdtoString(tg_nid_type tid)
{
    const std::string strUnknown = "TID not found";
    const std::pair<tg_nid_type, std::string> stypes[] =
            {
                    std::make_pair(FT_FILENAME,             std::string("FT_FILENAME")),
                    std::make_pair(FT_FILESIZE,             std::string("FT_FILESIZE")),
                    std::make_pair(FT_FILESIZE_HI,          std::string("FT_FILESIZE_HI")),
                    std::make_pair(FT_FILETYPE,             std::string("FT_FILETYPE")),
                    std::make_pair(FT_FILEFORMAT,           std::string("FT_FILEFORMAT")),
                    std::make_pair(FT_LASTSEENCOMPLETE,     std::string("FT_LASTSEENCOMPLETE")),
                    std::make_pair(FT_TRANSFERRED,          std::string("FT_TRANSFERRED")),
                    std::make_pair(FT_GAPSTART,             std::string("FT_GAPSTART")),
                    std::make_pair(FT_GAPEND,               std::string("FT_GAPEND")),
                    std::make_pair(FT_PARTFILENAME,         std::string("FT_PARTFILENAME")),
                    std::make_pair(FT_OLDDLPRIORITY,        std::string("FT_OLDDLPRIORITY")),
                    std::make_pair(FT_STATUS,               std::string("FT_STATUS")),
                    std::make_pair(FT_SOURCES,              std::string("FT_SOURCES")),
                    std::make_pair(FT_PERMISSIONS,          std::string("FT_PERMISSIONS")),
                    std::make_pair(FT_OLDULPRIORITY,        std::string("FT_PERMISSIONS")),
                    std::make_pair(FT_DLPRIORITY,           std::string("FT_DLPRIORITY")),
                    std::make_pair(FT_ULPRIORITY,           std::string("FT_ULPRIORITY")),
                    std::make_pair(FT_KADLASTPUBLISHKEY,    std::string("FT_KADLASTPUBLISHKEY")),
                    std::make_pair(FT_KADLASTPUBLISHSRC,    std::string("FT_KADLASTPUBLISHSRC")),
                    std::make_pair(FT_FLAGS,                std::string("FT_FLAGS")),
                    std::make_pair(FT_DL_ACTIVE_TIME,       std::string("FT_DL_ACTIVE_TIME")),
                    std::make_pair(FT_CORRUPTEDPARTS,       std::string("FT_CORRUPTEDPARTS")),
                    std::make_pair(FT_DL_PREVIEW,           std::string("FT_DL_PREVIEW")),
                    std::make_pair(FT_KADLASTPUBLISHNOTES,  std::string("FT_KADLASTPUBLISHNOTES")),
                    std::make_pair(FT_AICH_HASH,            std::string("FT_AICH_HASH")),
                    std::make_pair(FT_COMPLETE_SOURCES,     std::string("FT_COMPLETE_SOURCES")),
                    std::make_pair(FT_PUBLISHINFO,          std::string("FT_PUBLISHINFO")),
                    std::make_pair(FT_ATTRANSFERRED,        std::string("FT_ATTRANSFERRED")),
                    std::make_pair(FT_ATREQUESTED,          std::string("FT_ATREQUESTED")),
                    std::make_pair(FT_ATACCEPTED,           std::string("FT_ATACCEPTED")),
                    std::make_pair(FT_CATEGORY,             std::string("FT_CATEGORY")),
                    std::make_pair(FT_ATTRANSFERREDHI,      std::string("FT_ATTRANSFERREDHI")),
                    std::make_pair(FT_MEDIA_ARTIST,         std::string("FT_MEDIA_ARTIST")),
                    std::make_pair(FT_MEDIA_ALBUM,          std::string("FT_MEDIA_ALBUM")),
                    std::make_pair(FT_MEDIA_TITLE,          std::string("FT_MEDIA_TITLE")),
                    std::make_pair(FT_MEDIA_LENGTH,         std::string("FT_MEDIA_LENGTH")),
                    std::make_pair(FT_MEDIA_BITRATE,        std::string("FT_MEDIA_BITRATE")),
                    std::make_pair(FT_MEDIA_CODEC,          std::string("FT_MEDIA_CODEC")),
                    std::make_pair(FT_FILERATING,           std::string("FT_FILERATING")),
                    std::make_pair(ST_SERVERNAME,           std::string("ST_SERVERNAME")),
                    std::make_pair(ST_DESCRIPTION,          std::string("ST_DESCRIPTION")),
                    std::make_pair(ST_PING,                 std::string("ST_PING")),
                    std::make_pair(ST_FAIL,                 std::string("ST_FAIL")),
                    std::make_pair(ST_PREFERENCE,           std::string("ST_PREFERENCE")),
                    std::make_pair(ST_DYNIP,                std::string("ST_DYNIP")),
                    std::make_pair(ST_LASTPING_DEPRECATED,  std::string("ST_LASTPING_DEPRECATED")),
                    std::make_pair(ST_MAXUSERS,             std::string("ST_MAXUSERS")),
                    std::make_pair(ST_SOFTFILES,            std::string("ST_SOFTFILES")),
                    std::make_pair(ST_HARDFILES,            std::string("ST_HARDFILES")),
                    std::make_pair(ST_LASTPING,             std::string("ST_LASTPING")),
                    std::make_pair(ST_VERSION,              std::string("ST_VERSION")),
                    std::make_pair(ST_UDPFLAGS,             std::string("ST_UDPFLAGS")),
                    std::make_pair(ST_AUXPORTSLIST,         std::string("ST_AUXPORTSLIST")),
                    std::make_pair(ST_LOWIDUSERS,           std::string("ST_LOWIDUSERS")),
                    std::make_pair(ST_UDPKEY,               std::string("ST_UDPKEY")),
                    std::make_pair(ST_UDPKEYIP,             std::string("ST_UDPKEYIP")),
                    std::make_pair(ST_TCPPORTOBFUSCATION,   std::string("ST_TCPPORTOBFUSCATION")),
                    std::make_pair(ST_UDPPORTOBFUSCATION,   std::string("ST_UDPPORTOBFUSCATION")),
                    std::make_pair(CT_NAME,                 std::string("CT_NAME")),
                    std::make_pair(CT_SERVER_UDPSEARCH_FLAGS,std::string("CT_SERVER_UDPSEARCH_FLAGS")),
                    std::make_pair(CT_PORT,                 std::string("CT_PORT")),
                    std::make_pair(CT_VERSION,              std::string("CT_VERSION")),
                    std::make_pair(CT_SERVER_FLAGS,         std::string("CT_SERVER_FLAGS")),
                    std::make_pair(CT_EMULECOMPAT_OPTIONS,  std::string("CT_EMULECOMPAT_OPTIONS")),
                    std::make_pair(CT_EMULE_RESERVED1,      std::string("CT_EMULE_RESERVED1")),
                    std::make_pair(CT_EMULE_RESERVED2,      std::string("CT_EMULE_RESERVED2")),
                    std::make_pair(CT_EMULE_RESERVED3,      std::string("CT_EMULE_RESERVED3")),
                    std::make_pair(CT_EMULE_RESERVED4,      std::string("CT_EMULE_RESERVED4")),
                    std::make_pair(CT_EMULE_RESERVED5,      std::string("CT_EMULE_RESERVED5")),
                    std::make_pair(CT_EMULE_RESERVED6,      std::string("CT_EMULE_RESERVED6")),
                    std::make_pair(CT_EMULE_RESERVED7,      std::string("CT_EMULE_RESERVED7")),
                    std::make_pair(CT_EMULE_RESERVED8,      std::string("CT_EMULE_RESERVED8")),
                    std::make_pair(CT_EMULE_RESERVED9,      std::string("CT_EMULE_RESERVED9")),
                    std::make_pair(CT_EMULE_UDPPORTS,       std::string("CT_EMULE_UDPPORTS")),
                    std::make_pair(CT_EMULE_MISCOPTIONS1,   std::string("CT_EMULE_MISCOPTIONS1")),
                    std::make_pair(CT_EMULE_VERSION,        std::string("CT_EMULE_VERSION")),
                    std::make_pair(CT_EMULE_BUDDYIP,        std::string("CT_EMULE_BUDDYIP")),
                    std::make_pair(CT_EMULE_BUDDYUDP,       std::string("CT_EMULE_BUDDYUDP")),
                    std::make_pair(CT_EMULE_MISCOPTIONS2,   std::string("CT_EMULE_MISCOPTIONS2")),
                    std::make_pair(CT_EMULE_RESERVED13,     std::string("CT_EMULE_RESERVED13"))
            };

    for (size_t n = 0; n < sizeof(stypes)/sizeof(stypes[0]); n++)
    {
        if (stypes[n].first == tid)
        {
            return (stypes[n].second);
        }
    }

    return (strUnknown);
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
    DBG("name: " << m_strName.c_str() << " tag id: " << taggIdtoString(m_nNameId));

    switch (getType())
    {
        case TAGTYPE_UINT64:
        case TAGTYPE_UINT32:
        case TAGTYPE_UINT16:
        case TAGTYPE_UINT8:
            DBG("VALUE: " << asInt());
            break;
        case TAGTYPE_FLOAT32:
            DBG("VALUE: " << asFloat());
            break;
        case TAGTYPE_BOOL:
            DBG("VALUE: " << (asBool() ? "true" : "false"));
            break;
        case TAGTYPE_HASH16:
            DBG("VALUE: " << asHash().toString());
            break;
        case TAGTYPE_BLOB:
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
            DBG("VALUE" << asString());
            break;
        default:
            DBG("Unknown type");
            break;
    }
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

const md4_hash& base_tag::asHash() const
{
    throw libed2k_exception(errors::incompatible_tag_getter);
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
    m_strValue = bom_filter(m_strValue);
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
			catch(std::ios_base::failure&)
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
