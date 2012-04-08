#ifndef __LIBED2K_TAG__
#define __LIBED2K_TAG__

#include <boost/cstdint.hpp>

namespace libed2k
{

class tag
{
public:
    enum type
    {
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
        TAGTYPE_STR17,
                // accepted by eMule 0.42f (02-Mai-2004) in receiving code
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

    /**
      * copy constructor
     */
    tag(const tag& t);

    /**
      * copy
     */
    tag& operator=(const tag& t);

    /**
      * return current tag type
     */
    tag::type getType() const
    {
        return m_nType;
    }

    /**
      * check tag type
     */
    bool isStr() const
    {
        return m_nType == TAGTYPE_STRING;
    }

    bool isInt() const
    {
        return (m_nType == TAGTYPE_UINT64) || (m_nType == TAGTYPE_UINT32) || (m_nType == TAGTYPE_UINT16) || (m_nType == TAGTYPE_UINT8);
    }

    bool isFloat() const
    {
        return m_nType == TAGTYPE_FLOAT32;
    }

    bool isHash() const
    {
        return m_nType == TAGTYPE_HASH16;
    }

    bool isBlob() const
    {
        return m_nType == TAGTYPE_BLOB;
    }

    bool isBsob() const
    {
        return m_nType == TAGTYPE_BSOB;
    }

private:
    tag();
    type m_nType;

    union
    {
        md4_hash        m_hashValue;
        std::string     m_pstrValue;
        boost::uint64_t m_nValue;
        float           m_fValue;
        unsigned char*  m_pData;
    };

};

}


#endif
