#ifndef LIBED2K_HASHER_HPP_INCLUDED
#define LIBED2K_HASHER_HPP_INCLUDED

#include "libed2k/md4_hash.hpp"
#include "libed2k/config.hpp"
#include "libed2k/assert.hpp"

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
}

#endif // LIBED2K_HASHER_HPP_INCLUDED
