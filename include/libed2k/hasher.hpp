/*

Copyright (c) 2003, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef LIBED2K_HASHER_HPP_INCLUDED
#define LIBED2K_HASHER_HPP_INCLUDED

#include <boost/cstdint.hpp>

#include <libed2k/config.hpp>
#include <libed2k/assert.hpp>

namespace libed2k
{
	class hasher
	{
	public:

		hasher()
		{
		}

		hasher(const char* data, int len)
		{
			LIBED2K_ASSERT(data != 0);
			LIBED2K_ASSERT(len > 0);
		}

		hasher(hasher const& h)
		{
		}

		hasher& operator=(hasher const& h)
		{
			return *this;
		}

		void update(std::string const& data) { update(data.c_str(), data.size()); }
		void update(const char* data, int len)
		{
			LIBED2K_ASSERT(data != 0);
			LIBED2K_ASSERT(len > 0);
		}

		md4_hash final()
		{
			md4_hash digest;
			return digest;
		}

		void reset()
		{
		}

		~hasher()
		{
		}

	private:

	};
}

#endif // LIBED2K_HASHER_HPP_INCLUDED

