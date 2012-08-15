/*

Copyright (c) 2007, Arvid Norberg
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

#ifndef LIBED2K_ASSERT

#include <libed2k/config.hpp>

#if (!defined LIBED2K_DEBUG && !LIBED2K_RELEASE_ASSERTS) \
	|| LIBED2K_NO_ASSERTS
#define LIBED2K_ASSERT(a) do {} while(false)
#define LIBED2K_ASSERT_VAL(a, b) do {} while(false)
#else

#if LIBED2K_PRODUCTION_ASSERTS
extern char const* libtorrent_assert_log;
#endif

#include <string>

std::string demangle(char const* name);
void print_backtrace(char* out, int len, int max_depth = 0);

#if (defined __linux__ || defined __MACH__) && defined __GNUC__ && !LIBED2K_USE_SYSTEM_ASSERT

#if LIBED2K_USE_IOSTREAM
#include <sstream>
#endif

LIBED2K_EXPORT void assert_fail(const char* expr, int line, char const* file
	, char const* function, char const* val);

#define LIBED2K_ASSERT(x) do { if (x) {} else assert_fail(#x, __LINE__, __FILE__, __PRETTY_FUNCTION__, 0); } while (false)
#if LIBED2K_USE_IOSTREAM
#define LIBED2K_ASSERT_VAL(x, y) do { if (x) {} else { std::stringstream __s__; __s__ << #y ": " << y; assert_fail(#x, __LINE__, __FILE__, __PRETTY_FUNCTION__, __s__.str().c_str()); } } while (false)
#else
#define LIBED2K_ASSERT_VAL(x, y) LIBED2K_ASSERT(x)
#endif

#else
#include <cassert>
#define LIBED2K_ASSERT(x) assert(x)
#define LIBED2K_ASSERT_VAL(x, y) assert(x)
#endif

#endif

#endif
