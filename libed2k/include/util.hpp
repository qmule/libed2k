
#ifndef __LIBED2K_UTIL__
#define __LIBED2K_UTIL__

namespace libed2k {

    template <typename A, typename B>
    A div_ceil(A a, B b)
    {
        return A(a/b + bool(a%b));
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
}

#endif
