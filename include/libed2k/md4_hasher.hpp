
#ifndef LIBED2K_HASHER_HPP_INCLUDED
#define LIBED2K_HASHER_HPP_INCLUDED

#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <cryptopp/md4.h>

#include <libed2k/md4_hash.hpp>
#include <libed2k/peer_id.hpp>
#include <libed2k/config.hpp>
#include <libed2k/assert.hpp>

namespace libed2k
{
	class md4_hasher
	{
	public:

		md4_hasher()
		{
		}

		md4_hasher(const char* data, int len)
		{
			LIBED2K_ASSERT(data != 0);
			LIBED2K_ASSERT(len > 0);
			update(data, len);
		}

		md4_hasher(md4_hasher const& h) : m_hasher(h.m_hasher)
		{
		}

		md4_hasher& operator=(md4_hasher const& h)
		{
		    m_hasher = h.m_hasher;
			return *this;
		}

		void update(std::string const& data) { update(data.c_str(), data.size()); }
		void update(const char* data, int len)
		{
			LIBED2K_ASSERT(data != 0);
			LIBED2K_ASSERT(len > 0);
			m_hasher.Update(reinterpret_cast<const unsigned char*>(data), len);
		}

		md4_hash final()
		{
		    LIBED2K_ASSERT(m_hasher.DigestSize() == 16);
			md4_hash digest;
			m_hasher.Final(digest.getContainer());
			return digest;
		}

		void reset()
		{
		    m_hasher.Restart();
		}

		~md4_hasher()
		{
		}

	private:
		CryptoPP::Weak1::MD4 m_hasher;
	};
}

#endif // LIBED2K_HASHER_HPP_INCLUDED
