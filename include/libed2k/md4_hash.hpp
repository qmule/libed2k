
#ifndef __LIBED2K_MD4_HASH__
#define __LIBED2K_MD4_HASH__

#include <string>
#include <string.h>
#include <vector>
#include <sstream>

#include "libed2k/size_type.hpp"
#include "libed2k/assert.hpp"
#include "libed2k/escape_string.hpp"
#include "libed2k/archive.hpp"
#include "libed2k/md4.hpp"

namespace libed2k
{
    /**
      * this class simple container for hash array
     */
    class md4_hash
    {
    public:
        friend class archive::access;
        typedef boost::uint8_t md4hash_container[MD4_HASH_SIZE];
        static const md4_hash terminal;
        static const md4_hash libed2k;
        static const md4_hash emule;
        static const md4_hash invalid;

        enum { hash_size = MD4_HASH_SIZE };

        md4_hash()
        {
            clear();
        }

        md4_hash(const std::vector<boost::uint8_t>& vHash)
        {
            size_t nSize = (vHash.size()> MD4_HASH_SIZE)?MD4_HASH_SIZE:vHash.size();

            for (size_t i = 0; i < nSize; i++)
            {
                m_hash[i] = vHash[i];
            }
        }

        md4_hash(const md4hash_container& container)
        {
            memcpy(m_hash, container, MD4_HASH_SIZE);
        }

        bool defined() const
        {
            int sum = 0;
            for (size_t i = 0; i < MD4_HASH_SIZE; ++i)
                sum |= m_hash[i];
            return sum != 0;
        }

        unsigned char* getContainer()
        {
            return &m_hash[0];
        }

        bool operator==(const md4_hash& hash) const
        {
            return (memcmp(m_hash, hash.m_hash, MD4_HASH_SIZE) == 0);
        }

        bool operator!=(const md4_hash& hash) const
        {
            return (memcmp(m_hash, hash.m_hash, MD4_HASH_SIZE) != 0);
        }

        bool operator<(const md4_hash& hash) const
        {
            return (memcmp(m_hash, hash.m_hash, MD4_HASH_SIZE) < 0);
        }

        bool operator>(const md4_hash& hash) const
        {
             return (memcmp(m_hash, hash.m_hash, MD4_HASH_SIZE) > 0);
        }

        void clear()
        {
            memset(m_hash, 0, MD4_HASH_SIZE);
        }

        static md4_hash fromHashset(const std::vector<md4_hash>& hashset);

        static md4_hash fromString(const std::string& strHash)
        {
            LIBED2K_ASSERT(strHash.size() == MD4_HASH_SIZE*2);

            md4_hash hash;

            if (!from_hex(strHash.c_str(), MD4_HASH_SIZE*2, (char*)hash.m_hash))
            {
                hash = invalid;
            }

            return (hash);
        }

        std::string toString() const
        {
            static const char c[MD4_HASH_SIZE] =
                    {
                            '\x30', '\x31', '\x32', '\x33',
                            '\x34', '\x35', '\x36', '\x37',
                            '\x38', '\x39', '\x41', '\x42',
                            '\x43', '\x44', '\x45', '\x46'
                    };

            std::string res(MD4_HASH_SIZE*2, '\x00');
            LIBED2K_ASSERT(res.length() == MD4_HASH_SIZE*2);

            for (size_t i = 0; i < MD4_HASH_SIZE; ++i)
            {
                res[i*2]        = c[(m_hash[i] >> 4)];
                res[(i*2)+1]    = c[(m_hash[i] & 0x0F)];
            }

            return res;
        }

        boost::uint8_t operator[](size_t n) const
        {
            LIBED2K_ASSERT(n < MD4_HASH_SIZE);
            return (m_hash[n]);
        }

        boost::uint8_t& operator[](size_t n)
        {
            LIBED2K_ASSERT(n < MD4_HASH_SIZE);
            return (m_hash[n]);
        }

        template<typename Archive>
        void serialize(Archive& ar)
        {
            for (size_t n = 0; n < sizeof(md4hash_container); n++)
            {
                ar & m_hash[n];
            }
        }

        friend std::ostream& operator<< (std::ostream& stream, const md4_hash& hash);
        void dump() const;

    private:
        md4hash_container   m_hash;
    };
}

#endif
