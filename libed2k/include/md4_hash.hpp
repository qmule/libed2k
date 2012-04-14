
#ifndef __LIBED2K_MD4_HASH__
#define __LIBED2K_MD4_HASH__

#include <boost/cstdint.hpp>
#include <boost/assert.hpp>
#include <string>
#include <sstream>
#include <string.h>
#include <algorithm>
#include <typeinfo>
#include <vector>
#include "archive.hpp"
#include "error_code.hpp"

namespace libed2k{

    const size_t MD4_HASH_SIZE = 16;

    /**
      * this class simple container for hash array
     */
    class md4_hash
    {
    public:
        friend class archive::access;
    	typedef boost::uint8_t md4hash_container[MD4_HASH_SIZE];
    	static const md4hash_container m_emptyMD4Hash;

    	md4_hash()
    	{
    	    clear();
    	}

    	md4_hash(const std::string strHash)
    	{
    	    fromString(strHash);
    	}

    	md4_hash(const std::vector<boost::uint8_t>& vHash)
    	{
    		size_t nSize = std::max(vHash.size(), MD4_HASH_SIZE);
    		for (size_t i = 0; i < nSize; i++)
    		{
    			m_hash[i] = vHash[i];
    		}
    	}

    	md4_hash(const md4hash_container& container)
    	{
    		memcpy(m_hash, container, MD4_HASH_SIZE);
    	}

    	bool operator==(const md4_hash& hash) const
        {
    	    return (memcmp(m_hash, hash.m_hash, MD4_HASH_SIZE) == 0);
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

    	void fromString(const std::string& strHash)
    	{
    	    if (strHash.size() < MD4_HASH_SIZE*2)
    	    {
    	        throw libed2k_exception(errors::md4_hash_index_error);
    	    }

    	    clear();

    	    for ( size_t i = 0; i < MD4_HASH_SIZE * 2; i++ )
    	    {
    	        unsigned int word = strHash[i];

                if ((word >= '0') && (word <= '9'))
                {
                    word -= '0';
                }
                else if ((word >= 'A') && (word <= 'F'))
                {
                    word -= 'A' - 10;
                }
                else if (word >= 'a' && word <= 'f')
                {
                    word -= 'a' - 10;
                }
                else
                {
                     throw libed2k_exception(errors::md4_hash_convert_error);
                }                

                unsigned char cData = static_cast<unsigned char>(word);

                if (i % 2 == 0)
                {
                    m_hash[i/2] = static_cast<unsigned char>(cData << 4);
                }
                else
                {
                    m_hash[i/2] += static_cast<unsigned char>(cData);
                }
    	    }
    	}

    	std::string toString() const
    	{
    	    std::stringstream ss;

    	    for (size_t i = 0; i < MD4_HASH_SIZE; i++)
    	    {
    	        ss << std::uppercase << std::hex << (m_hash[i] >> 4) << (m_hash[i] & 0x0F);
    	    }

    	    return (ss.str());
    	}

    	boost::uint8_t operator[](size_t n) const
    	{
    	    if (n >= MD4_HASH_SIZE)
            {
                throw libed2k_exception(errors::md4_hash_index_error);
            }

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

    private:
    	md4hash_container   m_hash;
    };

}



#endif
