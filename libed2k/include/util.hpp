
#ifndef __LIBED2K_UTIL__
#define __LIBED2K_UTIL__

#include "md4_hash.hpp"
#include "types.hpp"

namespace libed2k {

    template <typename A, typename B>
    A div_ceil(A a, B b)
    {
        return A(a/b + bool(a%b));
    }

    template <typename A>
    A bits2bytes(A bits)
    {
        return (bits + 7) / 8;
    }

    inline int round_up8(int v)
    {
        return ((v & 7) == 0) ? v : v + (8 - (v & 7));
    }

    inline ptime time_now()
    {
        return time::microsec_clock::universal_time();
    }

    template <typename C>
    class cyclic_iterator {
        typedef typename C::iterator I;
        I m_it;
        C& m_cont;
    public:
        cyclic_iterator(C& c):
            m_it(c.begin()), m_cont(c) {
        }

        typename C::value_type* operator->() {
            validate();
            return m_it.operator->();
        }

        cyclic_iterator& operator++() {
            ++m_it;
            validate();
            return *this;
        }
    private:
        void validate() {
            if (m_it == m_cont.end()) m_it = m_cont.begin();
        }
    };

    md4_hash hash_md4(const std::string& str);

    #define HIGHEST_LOWID_ED2K      16777216

    inline bool isLowId(boost::uint32_t nId)
    {
        return (nId < HIGHEST_LOWID_ED2K);
    }
}

#endif
