// Copyright Daniel Wallin 2004. Use, modification and distribution is
// subject to the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef LIBED2K_INVARIANT_ACCESS_HPP_INCLUDED
#define LIBED2K_INVARIANT_ACCESS_HPP_INCLUDED

#include "libed2k/assert.hpp"

namespace libed2k
{

	class invariant_access
	{
	public:
		template<class T>
		static void check_invariant(T const& self)
		{
			self.check_invariant();
		}
	};

	template<class T>
	void libed2k_check_invariant(T const& x)
	{
		invariant_access::check_invariant(x);
	}

	struct invariant_checker {};

	template<class T>
	struct invariant_checker_impl : invariant_checker
	{
		invariant_checker_impl(T const& self_)
			: self(self_)
		{
			try
			{
				libed2k_check_invariant(self);
			}
			catch (...)
			{
				LIBED2K_ASSERT(false);
			}
		}

		~invariant_checker_impl()
		{
			try
			{
				libed2k_check_invariant(self);
			}
			catch (...)
			{
				LIBED2K_ASSERT(false);
			}
		}

		T const& self;
	};

	template<class T>
	invariant_checker_impl<T> libed2k_make_invariant_checker(T const& x)
	{
		return invariant_checker_impl<T>(x);
	}
}

#if defined LIBED2K_DEBUG && !defined LIBED2K_DISABLE_INVARIANT_CHECKS
#define INVARIANT_CHECK \
	invariant_checker const& _invariant_check = libed2k_make_invariant_checker(*this); \
	(void)_invariant_check; \
	do {} while (false)
#else
#define INVARIANT_CHECK do {} while (false)
#endif

#endif // LIBED2K_INVARIANT_ACCESS_HPP_INCLUDED
