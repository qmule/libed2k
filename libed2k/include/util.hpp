
#ifndef __LIBED2K_UTIL__
#define __LIBED2K_UTIL__

namespace libed2k {

    template <typename A, typename B>
    A div_ceil(A a, B b)
    {
        return A(a/b + (a%b == 0 ? 0 : 1));
    }

    inline int round_up8(int v)
    {
        return ((v & 7) == 0) ? v : v + (8 - (v & 7));
    }


}

#endif
