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


class tag
{
public:
    friend class libed2k::archive::access;

    enum tag_types
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

    typedef std::vector<unsigned char> binary_array;

    tag() : m_nType(TAGTYPE_UNDEFINED)
    {
        m_vValue.reserve(16);
    }

    tag(const tag& t);

    inline bool isDefined() const
    {
        return (m_nType != TAGTYPE_UNDEFINED);
    }

    inline bool isStr() const
    {
        return (m_nType == TAGTYPE_STRING);
    }

    inline bool isInt() const
    {
        return (m_nType == TAGTYPE_UINT64) || (m_nType == TAGTYPE_UINT32) || (m_nType == TAGTYPE_UINT16) || (m_nType == TAGTYPE_UINT8);
    }

    inline bool isFloat() const
    {
        return (m_nType == TAGTYPE_FLOAT32);
    }

    inline bool isHash() const
    {
        return (m_nType == TAGTYPE_HASH16);
    }

    inline bool isBlob() const
    {
        return (m_nType == TAGTYPE_BLOB);
    }

    inline bool isBsob() const
    {
        return (m_nType == TAGTYPE_BSOB);
    }

    inline boost::uint64_t getInt() const
    {
        CHECK_TAG_TYPE(isInt());
        return (m_nValue);
    }

    inline float getFloat() const
    {
        CHECK_TAG_TYPE(isFloat());
        float fRes;
        memcpy(&fRes, &m_vValue[0], 4); // is it correct?
        return (fRes);
    }

    inline boost::uint8_t getType() const
    {
        return m_nType;
    }

    const std::string getString() const
    {
        CHECK_TAG_TYPE(isStr());
        std::string strRes(m_vValue.begin(), m_vValue.end());
        return (strRes);
    }

    const binary_array& getBsob() const
    {
        CHECK_TAG_TYPE(isBsob());
        return (m_vValue);
    }

    const binary_array& getBlob() const
    {
        CHECK_TAG_TYPE(isBlob());
        return (m_vValue);
    }

    inline boost::uint8_t getNameID() const
    {
        return m_nNameID;
    }

    inline const std::string& getName() const
    {
        return m_strName;
    }

    bool operator==(const tag& t) const
    {
        // compare only defined tags with equal types
        if (!isDefined() || !t.isDefined() || (m_nType != t.getType()))
        {
            return (false);
        }

        if (isInt())
        {
            return (m_nValue == t.m_nValue);
        }

        return (m_vValue == t.m_vValue);
    }

    template<typename Archive>
    void save(Archive& ar) const
    {
        // we can't save undefined tag anywhere
        if (!isDefined())
        {
            throw libed2k::libed2k_exception(libed2k::errors::save_undefined_tag_error);
        }

        // local tag type
        boost::uint8_t nType;

        if (isInt())
        {
            if (m_nValue <= 0xFFL)
            {
                nType = TAGTYPE_UINT8;
            }
            else if (m_nValue <= 0xFFFFL)
            {
                nType = TAGTYPE_UINT16;
            }
            else if (m_nValue <= 0xFFFFFFFFL)
            {
                nType = TAGTYPE_UINT32;
            }
            else
            {
                nType = TAGTYPE_UINT64;
            }
        }
        else if (isStr())
        {
            if (m_vValue.size() >= 1 && m_vValue.size() <= 16)
            {
                nType = static_cast<boost::uint8_t>(TAGTYPE_STR1 + m_vValue.size());
                --nType;
            }
            else
            {
                nType = TAGTYPE_STRING;
            }
        }
        else
        {
            nType = m_nType;
        }

        if (m_strName.empty())
        {
            boost::uint8_t nSaveType = nType | 0x80;
            ar & nSaveType;
            ar & m_nNameID;
        }
        else
        {
            boost::uint16_t nLength = static_cast<boost::uint16_t>(m_strName.size());
            ar & nType;
            ar & nLength;
            ar & m_strName;
        }

        // ok, write data
        switch (nType)
        {
            case TAGTYPE_UINT64:
                ar & m_nValue;
                break;
            case TAGTYPE_UINT32:
            {
                boost::uint32_t nVal = static_cast<boost::uint32_t>(m_nValue);
                ar & nVal;
                break;
            }
            case TAGTYPE_UINT16:
            {
                boost::uint16_t nVal = static_cast<boost::uint16_t>(m_nValue);
                ar & nVal;
                break;
            }
            case TAGTYPE_UINT8:
            {
                boost::uint8_t nVal = static_cast<boost::uint8_t>(m_nValue);
                ar & nVal;
                break;
            }
            case TAGTYPE_FLOAT32:
            {
                binary_write(ar); // is it correct
                break;
            }
            case TAGTYPE_STRING:
            {
                boost::uint16_t nSize = static_cast<boost::uint16_t>(m_vValue.size());
                ar & nSize;
                binary_write(ar);
                break;
            }
            case TAGTYPE_BLOB:
            {
                boost::uint32_t nSize = m_vValue.size();
                ar & nSize;
            }
            case TAGTYPE_HASH16:
                binary_write(ar);
                break;
            default:
                // See comment on the default: of CTag::CTag(const CFileDataIO& data, bool bOptUTF8)
                if (nType >= TAGTYPE_STR1 && nType <= TAGTYPE_STR16)
                {
                    // do not write string size
                    binary_write(ar);
                }
                else
                {
                    throw libed2k::libed2k_exception(libed2k::errors::invalid_tag_type);
                }
                break;
        }

    }

    template<typename Archive>
    void load(Archive& ar)
    {
        ar & m_nType;

        if (m_nType & 0x80)
        {
            m_nType &= 0x7F;
            ar & m_nNameID;
        }
        else
        {
            boost::uint16_t nLength;
            ar & nLength;

            if (nLength == 1)
            {
                ar & m_nNameID;
            }
            else
            {
                m_nNameID = 0;
                m_strName.resize(nLength);
                ar & m_strName;
            }
        }

        boost::uint16_t nDataLength = 0;
        boost::uint32_t nBlobLength = 0;

        switch (m_nType)
        {
            case TAGTYPE_UINT64:
            ar & m_nValue;
            break;

            case TAGTYPE_UINT32:
            {
                boost::uint32_t nTemp;
                ar & nTemp;
                m_nValue = static_cast<boost::uint64_t>(nTemp);
                break;
            }

            case TAGTYPE_UINT16:
            {
                boost::uint16_t nTemp;
                ar & nTemp;
                m_nValue = static_cast<boost::uint64_t>(nTemp);
                //m_nType = TAGTYPE_UINT32;
                break;
            }

            case TAGTYPE_UINT8:
            {
                boost::uint8_t nTemp;
                ar & nTemp;
                m_nValue = static_cast<boost::uint64_t>(nTemp);
                //m_nType = TAGTYPE_UINT32;
                break;
            }

            case TAGTYPE_FLOAT32:
                nDataLength = 4;
                break;

            case TAGTYPE_STRING:
                ar & nDataLength;
                break;

            case TAGTYPE_HASH16:
                nDataLength = MD4_HASH_SIZE;
                break;

            case TAGTYPE_BOOL:
                nDataLength = sizeof(boost::uint8_t);
                break;
            case TAGTYPE_BOOLARRAY:
            {
                ar & nDataLength;
                ar.shift(nDataLength/8 + 1);
                nDataLength = 0;
                // 07-Apr-2004: eMule versions prior to 0.42e.29 used the formula "(len+7)/8"!
                //#warning This seems to be off by one! 8 / 8 + 1 == 2, etc.
                break;
            }

            case TAGTYPE_BLOB:
                // 07-Apr-2004: eMule versions prior to 0.42e.29 handled the "len" as int16!
                ar & nBlobLength;
                break;
            default:
                if (m_nType >= TAGTYPE_STR1 && m_nType <= TAGTYPE_STR16)
                {
                    nDataLength = static_cast<boost::uint16_t>(m_nType - TAGTYPE_STR1);
                    ++nDataLength;
                    m_nType = TAGTYPE_STRING;
                }
                else
                {
                    // Since we cannot determine the length of this tag, we
                    // simply have to abort reading the file.
                    throw libed2k::libed2k_exception(libed2k::errors::invalid_tag_type);
                }
                break;
        }

        if (nDataLength > 0)
        {
            binary_read(ar, static_cast<size_t>(nDataLength));
        }
        else if (nBlobLength > 0)
        {
            // avoid huge memory allocation on incorrect tags
            if (nBlobLength > 65535)
            {
                int nSignLength = -1*nBlobLength;
                ar.container().seekg(nBlobLength, std::ios::cur);   // go to end of blob

                if (!ar.container().good())
                {
                    throw libed2k::libed2k_exception(libed2k::errors::blob_tag_too_long);
                }

                ar.container().seekg(nSignLength, std::ios::cur);   // go back

            }

            binary_read(ar, static_cast<size_t>(nBlobLength));
        }

    }

    LIBED2K_SERIALIZATION_SPLIT_MEMBER()

private:
    template<typename Archive>
    void binary_read(Archive& ar, size_t nSize)
    {
        m_vValue.resize(nSize);
        ar.raw_read(reinterpret_cast<char*>(&m_vValue[0]), nSize);
    }

    template<typename Archive>
    void binary_write(Archive& ar) const
    {
        ar.raw_write(reinterpret_cast<const char*>(&m_vValue[0]), m_vValue.size());
    }

    boost::uint8_t  m_nType;    //!< tag type
    boost::uint8_t  m_nNameID;  //!< tag number name id
    std::string     m_strName;  //!< tag string name
    binary_array    m_vValue;   //!< tag binary value
    boost::uint64_t m_nValue;   //!< tag common integer container
};

}

#endif // __CTAG__
