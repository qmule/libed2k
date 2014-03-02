#ifndef LIBED2K_HASHER_HPP_INCLUDED
#define LIBED2K_HASHER_HPP_INCLUDED

#include "libed2k/config.hpp"
#include "libed2k/assert.hpp"
#include "libed2k/peer_id.hpp"
#include "libed2k/archive.hpp"

#include <boost/cstdint.hpp>
#include <vector>

#ifdef LIBED2K_USE_GCRYPT
#include <gcrypt.h>
#elif defined LIBED2K_USE_OPENSSL
extern "C"
{
#include <openssl/sha.h>
#include <openssl/md4.h>
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

// from md4.h
#define MD4_DIGEST_LENGTH 16

struct LIBED2K_EXTRA_EXPORT MD4_CTX
{
    boost::uint32_t lo, hi;
    boost::uint32_t a, b, c, d;
    unsigned char buffer[64];
    boost::uint32_t block[MD4_DIGEST_LENGTH];
};

LIBED2K_EXTRA_EXPORT void MD4_Init(struct MD4_CTX *ctx);
LIBED2K_EXTRA_EXPORT void MD4_Update(struct MD4_CTX *ctx, boost::uint8_t const* data, boost::uint32_t size);
LIBED2K_EXTRA_EXPORT void MD4_Final(boost::uint8_t result[MD4_DIGEST_LENGTH], struct MD4_CTX *ctx);

#endif


namespace libed2k
{
    class md4_hash
        {
        public:
            friend class archive::access;
            typedef boost::uint8_t md4hash_container[MD4_DIGEST_LENGTH];

            static const md4_hash terminal;
            static const md4_hash libed2k;
            static const md4_hash emule;
            static const md4_hash invalid;

            static md4_hash fromHashset(const std::vector<md4_hash>& hashset);
            static md4_hash fromString(const std::string& strHash);

            md4_hash() { clear(); }
            md4_hash(const std::vector<boost::uint8_t>& vHash);
            md4_hash(const md4hash_container& container);
            bool defined() const;

            unsigned char* getContainer(){ return &m_hash[0]; }

            bool operator==(const md4_hash& hash) const {return (memcmp(m_hash, hash.m_hash, MD4_DIGEST_LENGTH) == 0);}
            bool operator!=(const md4_hash& hash) const{return (memcmp(m_hash, hash.m_hash, MD4_DIGEST_LENGTH) != 0);}
            bool operator<(const md4_hash& hash) const{return (memcmp(m_hash, hash.m_hash, MD4_DIGEST_LENGTH) < 0);}
            bool operator>(const md4_hash& hash) const {return (memcmp(m_hash, hash.m_hash, MD4_DIGEST_LENGTH) > 0);}

            void clear() {memset(m_hash, 0, MD4_DIGEST_LENGTH);}
            std::string toString() const;

            boost::uint8_t operator[](size_t n) const
            {
                LIBED2K_ASSERT(n < MD4_DIGEST_LENGTH);
                return (m_hash[n]);
            }

            boost::uint8_t& operator[](size_t n)
            {
                LIBED2K_ASSERT(n < MD4_DIGEST_LENGTH);
                return (m_hash[n]);
            }

            template<typename Archive>
            void serialize(Archive& ar)
            {
                for (size_t n = 0; n < sizeof(md4hash_container); n++){
                    ar & m_hash[n];
                }
            }

            friend std::ostream& operator<< (std::ostream& stream, const md4_hash& hash);
            void dump() const;
        private:
            md4hash_container   m_hash;
        };

    // NOTE - we use self implemented MD4 hash always since historical reasons
	class hasher
	{
	public:

		hasher(){ MD4_Init(&m_context); }
		hasher(const char* data, int len)
		{
			LIBED2K_ASSERT(data != 0);
			LIBED2K_ASSERT(len > 0);
			MD4_Init(&m_context);
			update(data, len);
		}

		hasher(hasher const& h){ m_context = h.m_context;}

		hasher& operator=(hasher const& h){
		    m_context = h.m_context;
			return *this;
		}

		void update(std::string const& data) { update(data.c_str(), data.size()); }
		void update(const char* data, int len){
			LIBED2K_ASSERT(data != 0);
			LIBED2K_ASSERT(len > 0);
			MD4_Update(&m_context, reinterpret_cast<const unsigned char*>(data), len);
		}

		md4_hash final(){
			md4_hash digest;
			MD4_Final(digest.getContainer(), &m_context);
			return digest;
		}

	private:
		MD4_CTX m_context;
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
