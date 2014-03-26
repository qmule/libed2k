/*
 * MD4 (RFC-1320) message digest.
 * Modified from MD5 code by Andrey Panin <pazke@donpac.ru>
 *
 * Written by Solar Designer <solar@openwall.com> in 2001, and placed in
 * the public domain.  There's absolutely no warranty.
 *
 * This differs from Colin Plumb's older public domain implementation in
 * that no 32-bit integer data type is required, there's no compile-time
 * endianness configuration, and the function prototypes match OpenSSL's.
 * The primary goals are portability and ease of use.
 *
 * This implementation is meant to be fast, but not as fast as possible.
 * Some known optimizations are not included to reduce source code size
 * and avoid compile-time configuration.
 */

#include "libed2k/hasher.hpp"
#include "libed2k/log.hpp"
#include <string.h>

#ifndef LIBED2K_USE_OPENSSL
namespace {
/*
 * The basic MD4 functions.
 */
#define F(x, y, z)	((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z)	(((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define H(x, y, z)	((x) ^ (y) ^ (z))

/*
 * The MD4 transformation for all four rounds.
 */
#define STEP(f, a, b, c, d, x, s) \
	(a) += f((b), (c), (d)) + (x);	 \
	(a) = ((a) << (s)) | ((a) >> (32 - (s)))


/*
 * SET reads 4 input bytes in little-endian byte order and stores them
 * in a properly aligned word in host byte order.
 *
 * The check for little-endian architectures which tolerate unaligned
 * memory accesses is just an optimization.  Nothing will break if it
 * doesn't work.
 */
#if defined(__i386__) || defined(__x86_64__)
#define SET(n) \
	(*(const boost::uint32_t *)&ptr[(n) * 4])
#define GET(n) \
	SET(n)
#else
#define SET(n) \
	(ctx->block[(n)] = \
	(boost::uint32_t)ptr[(n) * 4] | \
	((boost::uint32_t)ptr[(n) * 4 + 1] << 8) | \
	((boost::uint32_t)ptr[(n) * 4 + 2] << 16) | \
	((boost::uint32_t)ptr[(n) * 4 + 3] << 24))
#define GET(n) \
	(ctx->block[(n)])
#endif

/*
 * This processes one or more 64-byte data blocks, but does NOT update
 * the bit counters.  There're no alignment requirements.
 */
const unsigned char *body(struct MD4_CTX *ctx, const unsigned char *data, size_t size)
{
	const unsigned char *ptr;
	boost::uint32_t a, b, c, d;
	boost::uint32_t saved_a, saved_b, saved_c, saved_d;

	ptr = data;

	a = ctx->a;
	b = ctx->b;
	c = ctx->c;
	d = ctx->d;

	do {
		saved_a = a;
		saved_b = b;
		saved_c = c;
		saved_d = d;

/* Round 1 */
		STEP(F, a, b, c, d, SET( 0),  3);
		STEP(F, d, a, b, c, SET( 1),  7);
		STEP(F, c, d, a, b, SET( 2), 11);
		STEP(F, b, c, d, a, SET( 3), 19);

		STEP(F, a, b, c, d, SET( 4),  3);
		STEP(F, d, a, b, c, SET( 5),  7);
		STEP(F, c, d, a, b, SET( 6), 11);
		STEP(F, b, c, d, a, SET( 7), 19);

		STEP(F, a, b, c, d, SET( 8),  3);
		STEP(F, d, a, b, c, SET( 9),  7);
		STEP(F, c, d, a, b, SET(10), 11);
		STEP(F, b, c, d, a, SET(11), 19);

		STEP(F, a, b, c, d, SET(12),  3);
		STEP(F, d, a, b, c, SET(13),  7);
		STEP(F, c, d, a, b, SET(14), 11);
		STEP(F, b, c, d, a, SET(15), 19);
/* Round 2 */
		STEP(G, a, b, c, d, GET( 0) + 0x5A827999,  3);
		STEP(G, d, a, b, c, GET( 4) + 0x5A827999,  5);
		STEP(G, c, d, a, b, GET( 8) + 0x5A827999,  9);
		STEP(G, b, c, d, a, GET(12) + 0x5A827999, 13);

		STEP(G, a, b, c, d, GET( 1) + 0x5A827999,  3);
		STEP(G, d, a, b, c, GET( 5) + 0x5A827999,  5);
		STEP(G, c, d, a, b, GET( 9) + 0x5A827999,  9);
		STEP(G, b, c, d, a, GET(13) + 0x5A827999, 13);

		STEP(G, a, b, c, d, GET( 2) + 0x5A827999,  3);
		STEP(G, d, a, b, c, GET( 6) + 0x5A827999,  5);
		STEP(G, c, d, a, b, GET(10) + 0x5A827999,  9);
		STEP(G, b, c, d, a, GET(14) + 0x5A827999, 13);

		STEP(G, a, b, c, d, GET( 3) + 0x5A827999,  3);
		STEP(G, d, a, b, c, GET( 7) + 0x5A827999,  5);
		STEP(G, c, d, a, b, GET(11) + 0x5A827999,  9);
		STEP(G, b, c, d, a, GET(15) + 0x5A827999, 13);
/* Round 3 */
		STEP(H, a, b, c, d, GET( 0) + 0x6ED9EBA1,  3);
		STEP(H, d, a, b, c, GET( 8) + 0x6ED9EBA1,  9);
		STEP(H, c, d, a, b, GET( 4) + 0x6ED9EBA1, 11);
		STEP(H, b, c, d, a, GET(12) + 0x6ED9EBA1, 15);

		STEP(H, a, b, c, d, GET( 2) + 0x6ED9EBA1,  3);
		STEP(H, d, a, b, c, GET(10) + 0x6ED9EBA1,  9);
		STEP(H, c, d, a, b, GET( 6) + 0x6ED9EBA1, 11);
		STEP(H, b, c, d, a, GET(14) + 0x6ED9EBA1, 15);

		STEP(H, a, b, c, d, GET( 1) + 0x6ED9EBA1,  3);
		STEP(H, d, a, b, c, GET( 9) + 0x6ED9EBA1,  9);
		STEP(H, c, d, a, b, GET( 5) + 0x6ED9EBA1, 11);
		STEP(H, b, c, d, a, GET(13) + 0x6ED9EBA1, 15);

		STEP(H, a, b, c, d, GET( 3) + 0x6ED9EBA1,  3);
		STEP(H, d, a, b, c, GET(11) + 0x6ED9EBA1,  9);
		STEP(H, c, d, a, b, GET( 7) + 0x6ED9EBA1, 11);
		STEP(H, b, c, d, a, GET(15) + 0x6ED9EBA1, 15);

		a += saved_a;
		b += saved_b;
		c += saved_c;
		d += saved_d;

		ptr += 64;
	} while (size -= 64);

	ctx->a = a;
	ctx->b = b;
	ctx->c = c;
	ctx->d = d;

	return ptr;
}

#undef F
#undef G
#undef H

}
void MD4_Init(struct MD4_CTX *ctx)
{
	ctx->a = 0x67452301;
	ctx->b = 0xefcdab89;
	ctx->c = 0x98badcfe;
	ctx->d = 0x10325476;

	ctx->lo = 0;
	ctx->hi = 0;
}

void MD4_Update(struct MD4_CTX *ctx, boost::uint8_t const *data, boost::uint32_t size)
{
	/* @UNSAFE */
	boost::uint32_t saved_lo;
	unsigned long used, free;

	saved_lo = ctx->lo;
	if ((ctx->lo = (saved_lo + size) & 0x1fffffff) < saved_lo)
		ctx->hi++;
	ctx->hi += size >> 29;

	used = saved_lo & 0x3f;

	if (used) {
		free = 64 - used;

		if (size < free) {
			memcpy(&ctx->buffer[used], data, size);
			return;
		}

		memcpy(&ctx->buffer[used], data, free);
		data = (const unsigned char *) data + free;
		size -= free;
		body(ctx, ctx->buffer, 64);
	}

	if (size >= 64) {
		data = body(ctx, data, size & ~(unsigned long)0x3f);
		size &= 0x3f;
	}

	memcpy(ctx->buffer, data, size);
}

void MD4_Final(boost::uint8_t result[MD4_DIGEST_LENGTH], struct MD4_CTX *ctx)
{
	/* @UNSAFE */
	unsigned long used, free;

	used = ctx->lo & 0x3f;

	ctx->buffer[used++] = 0x80;

	free = 64 - used;

	if (free < 8) {
		memset(&ctx->buffer[used], 0, free);
		body(ctx, ctx->buffer, 64);
		used = 0;
		free = 64;
	}

	memset(&ctx->buffer[used], 0, free - 8);

	ctx->lo <<= 3;
	ctx->buffer[56] = ctx->lo;
	ctx->buffer[57] = ctx->lo >> 8;
	ctx->buffer[58] = ctx->lo >> 16;
	ctx->buffer[59] = ctx->lo >> 24;
	ctx->buffer[60] = ctx->hi;
	ctx->buffer[61] = ctx->hi >> 8;
	ctx->buffer[62] = ctx->hi >> 16;
	ctx->buffer[63] = ctx->hi >> 24;

	body(ctx, ctx->buffer, 64);

	result[0] = ctx->a;
	result[1] = ctx->a >> 8;
	result[2] = ctx->a >> 16;
	result[3] = ctx->a >> 24;
	result[4] = ctx->b;
	result[5] = ctx->b >> 8;
	result[6] = ctx->b >> 16;
	result[7] = ctx->b >> 24;
	result[8] = ctx->c;
	result[9] = ctx->c >> 8;
	result[10] = ctx->c >> 16;
	result[11] = ctx->c >> 24;
	result[12] = ctx->d;
	result[13] = ctx->d >> 8;
	result[14] = ctx->d >> 16;
	result[15] = ctx->d >> 24;

	memset(ctx, 0, sizeof(*ctx));
}
#endif

namespace libed2k
{
    const md4_hash md4_hash::terminal   = md4_hash::fromString("31D6CFE0D16AE931B73C59D7E0C089C0");
    const md4_hash md4_hash::libed2k    = md4_hash::fromString("31D6CFE0D14CE931B73C59D7E0C04BC0");
    const md4_hash md4_hash::emule      = md4_hash::fromString("31D6CFE0D10EE931B73C59D7E0C06FC0");
    const md4_hash md4_hash::invalid    = md4_hash::fromString("00000000000000000000000000000000");

    /*static*/
    md4_hash md4_hash::fromHashset(const std::vector<md4_hash>& hashset)
    {
        md4_hash result;

        if (hashset.size() > 1)
        {
            MD4_CTX ctx;
            MD4_Init(&ctx);
            MD4_Update(&ctx, reinterpret_cast<const boost::uint8_t*>(&hashset[0]),
                    hashset.size() * MD4_DIGEST_LENGTH);
            MD4_Final(result.getContainer(), &ctx);
        }
        else if (hashset.size() == 1)
        {
            result = hashset[0];
        }

        return result;
    }

    /*static*/
    md4_hash md4_hash::fromString(const std::string& strHash)
    {
        LIBED2K_ASSERT(strHash.size() == MD4_DIGEST_LENGTH*2);
        LIBED2K_ASSERT(strHash.size() == MD4_DIGEST_LENGTH*2);

        md4_hash hash;

        if (!from_hex(strHash.c_str(), MD4_DIGEST_LENGTH*2, (char*)hash.m_hash))
        {
            hash = invalid;
        }

        return (hash);
    }


    md4_hash md4_hash::max(){
        md4_hash ret;
        memset(ret.m_hash, 0xff, size);
        return ret;
    }

    md4_hash md4_hash::min(){
        md4_hash ret;
        memset(ret.m_hash, 0, size);
        return ret;
    }

    md4_hash::md4_hash(const std::vector<boost::uint8_t>& vHash){
        size_t nSize = (vHash.size()> MD4_DIGEST_LENGTH)?MD4_DIGEST_LENGTH:vHash.size();
        for (size_t i = 0; i < nSize; i++)
            m_hash[i] = vHash[i];
    }

    md4_hash::md4_hash(const md4hash_container& container){
        memcpy(m_hash, container, MD4_DIGEST_LENGTH);
    }

    bool md4_hash::is_all_zeros() const
    {
        for (const unsigned char* i = m_hash; i < m_hash+number_size; ++i)
            if (*i != 0) return false;
        return true;
    }

    md4_hash& md4_hash::operator<<=(int n)
    {
        LIBED2K_ASSERT(n >= 0);
        int num_bytes = n / 8;
        if (num_bytes >= number_size)
        {
            std::memset(m_hash, 0, number_size);
            return *this;
        }

        if (num_bytes > 0)
        {
            std::memmove(m_hash, m_hash + num_bytes, number_size - num_bytes);
            std::memset(m_hash + number_size - num_bytes, 0, num_bytes);
            n -= num_bytes * 8;
        }
        if (n > 0)
        {
            for (int i = 0; i < number_size - 1; ++i)
            {
                m_hash[i] <<= n;
                m_hash[i] |= m_hash[i+1] >> (8 - n);
            }
        }
        return *this;
    }

    md4_hash& md4_hash::operator>>=(int n)
    {
        LIBED2K_ASSERT(n >= 0);
        int num_bytes = n / 8;
        if (num_bytes >= number_size)
        {
            std::memset(m_hash, 0, number_size);
            return *this;
        }
        if (num_bytes > 0)
        {
            std::memmove(m_hash + num_bytes, m_hash, number_size - num_bytes);
            std::memset(m_hash, 0, num_bytes);
            n -= num_bytes * 8;
        }
        if (n > 0)
        {
            for (int i = number_size - 1; i > 0; --i)
            {
                m_hash[i] >>= n;
                m_hash[i] |= m_hash[i-1] << (8 - n);
            }
        }
        return *this;
    }

    bool md4_hash::operator==(md4_hash const& n) const
    {
        return std::equal(n.m_hash, n.m_hash+number_size, m_hash);
    }

    bool md4_hash::operator!=(md4_hash const& n) const
    {
        return !std::equal(n.m_hash, n.m_hash+number_size, m_hash);
    }

    bool md4_hash::operator<(md4_hash const& n) const
    {
        for (int i = 0; i < number_size; ++i)
        {
            if (m_hash[i] < n.m_hash[i]) return true;
            if (m_hash[i] > n.m_hash[i]) return false;
        }
        return false;
    }

    md4_hash md4_hash::operator~() const
    {
        md4_hash ret;
        for (int i = 0; i< number_size; ++i)
            ret.m_hash[i] = ~m_hash[i];
        return ret;
    }

    md4_hash md4_hash::operator^ (md4_hash const& n) const
    {
        md4_hash ret = *this;
        ret ^= n;
        return ret;
    }

    md4_hash md4_hash::operator& (md4_hash const& n) const
    {
        md4_hash ret = *this;
        ret &= n;
        return ret;
    }

    md4_hash& md4_hash::operator &= (md4_hash const& n)
    {
        for (int i = 0; i< number_size; ++i)
            m_hash[i] &= n.m_hash[i];
        return *this;
    }

    md4_hash& md4_hash::operator |= (md4_hash const& n)
    {
        for (int i = 0; i< number_size; ++i)
            m_hash[i] |= n.m_hash[i];
        return *this;
    }

    md4_hash& md4_hash::operator ^= (md4_hash const& n)
    {
        for (int i = 0; i< number_size; ++i)
            m_hash[i] ^= n.m_hash[i];
        return *this;
    }

    bool md4_hash::defined() const
    {
        int sum = 0;
        for (size_t i = 0; i < MD4_DIGEST_LENGTH; ++i)
            sum |= m_hash[i];
        return sum != 0;
    }

    std::string md4_hash::toString() const
    {
        static const char c[MD4_DIGEST_LENGTH] =
                {
                        '\x30', '\x31', '\x32', '\x33',
                        '\x34', '\x35', '\x36', '\x37',
                        '\x38', '\x39', '\x41', '\x42',
                        '\x43', '\x44', '\x45', '\x46'
                };

        std::string res(MD4_DIGEST_LENGTH*2, '\x00');
        LIBED2K_ASSERT(res.length() == MD4_DIGEST_LENGTH*2);

        for (size_t i = 0; i < MD4_DIGEST_LENGTH; ++i)
        {
            res[i*2]        = c[(m_hash[i] >> 4)];
            res[(i*2)+1]    = c[(m_hash[i] & 0x0F)];
        }

        return res;
    }

    void md4_hash::dump() const
    {
        DBG("md4_hash::dump " << toString().c_str());
    }
}

