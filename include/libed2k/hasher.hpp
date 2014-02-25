#ifndef LIBED2K_HASHER_HPP_INCLUDED
#define LIBED2K_HASHER_HPP_INCLUDED

#include "libed2k/md4_hash.hpp"
#include "libed2k/config.hpp"
#include "libed2k/assert.hpp"
#include "libed2k/peer_id.hpp"

#include <boost/cstdint.hpp>

#ifdef LIBED2K_USE_GCRYPT
#include <gcrypt.h>
#elif defined LIBED2K_USE_OPENSSL
extern "C"
{
#include <openssl/sha.h>
}
#else
// from sha1.cpp
struct LIBED2K_EXTRA_EXPORT SHA_CTX
{
    boost::uint32_t state[5];
    boost::uint32_t count[2];
    boost::uint8_t buffer[64];
};

LIBED2K_EXTRA_EXPORT void SHA1_Init(SHA_CTX* context);
LIBED2K_EXTRA_EXPORT void SHA1_Update(SHA_CTX* context, boost::uint8_t const* data, boost::uint32_t len);
LIBED2K_EXTRA_EXPORT void SHA1_Final(boost::uint8_t* digest, SHA_CTX* context);

#endif


namespace libed2k
{
	class hasher
	{
	public:

		hasher()
		{
		    reset();
		}

		hasher(const char* data, int len)
		{
			LIBED2K_ASSERT(data != 0);
			LIBED2K_ASSERT(len > 0);
			reset();
			update(data, len);
		}

		hasher(hasher const& h)
		{
		    m_context = h.m_context;
		}

		hasher& operator=(hasher const& h)
		{
		    m_context = h.m_context;
			return *this;
		}

		void update(std::string const& data) { update(data.c_str(), data.size()); }
		void update(const char* data, int len)
		{
			LIBED2K_ASSERT(data != 0);
			LIBED2K_ASSERT(len > 0);
			md4_update(&m_context, reinterpret_cast<const unsigned char*>(data), len);
		}

		md4_hash final()
		{
			md4_hash digest;
			md4_final(&m_context, digest.getContainer());
			return digest;
		}

		void reset()
		{
		    md4_init(&m_context);
		}

		~hasher()
		{
		}

	private:
		md4_context m_context;
	};

	class LIBED2K_EXTRA_EXPORT sha1_hasher
    {
    public:

        sha1_hasher()
        {
#ifdef LIBED2K_USE_GCRYPT
            gcry_md_open(&m_context, GCRY_MD_SHA1, 0);
#else
            SHA1_Init(&m_context);
#endif
        }
        sha1_hasher(const char* data, int len)
        {
            LIBED2K_ASSERT(data != 0);
            LIBED2K_ASSERT(len > 0);
#ifdef LIBED2K_USE_GCRYPT
            gcry_md_open(&m_context, GCRY_MD_SHA1, 0);
            gcry_md_write(m_context, data, len);
#else
            SHA1_Init(&m_context);
            SHA1_Update(&m_context, reinterpret_cast<unsigned char const*>(data), len);
#endif
        }

#ifdef LIBED2K_USE_GCRYPT
        sha1_hasher(sha1_hasher const& h)
        {
            gcry_md_copy(&m_context, h.m_context);
        }

        sha1_hasher& operator=(sha1_hasher const& h)
        {
            gcry_md_close(m_context);
            gcry_md_copy(&m_context, h.m_context);
            return *this;
        }
#endif

        sha1_hasher& update(std::string const& data) { update(data.c_str(), data.size()); return *this; }
        sha1_hasher& update(const char* data, int len)
        {
            LIBED2K_ASSERT(data != 0);
            LIBED2K_ASSERT(len > 0);
#ifdef LIBED2K_USE_GCRYPT
            gcry_md_write(m_context, data, len);
#else
            SHA1_Update(&m_context, reinterpret_cast<unsigned char const*>(data), len);
#endif
            return *this;
        }

        sha1_hash final()
        {
            sha1_hash digest;
#ifdef LIBED2K_USE_GCRYPT
            gcry_md_final(m_context);
            digest.assign((const char*)gcry_md_read(m_context, 0));
#else
            SHA1_Final(digest.begin(), &m_context);
#endif
            return digest;
        }

        void reset()
        {
#ifdef LIBED2K_USE_GCRYPT
            gcry_md_reset(m_context);
#else
            SHA1_Init(&m_context);
#endif
        }

#ifdef LIBED2K_USE_GCRYPT
        ~sha1_hasher()
        {
            gcry_md_close(m_context);
        }
#endif

    private:

#ifdef LIBED2K_USE_GCRYPT
        gcry_md_hd_t m_context;
#else
        SHA_CTX m_context;
#endif
    };
}

#endif // LIBED2K_HASHER_HPP_INCLUDED
