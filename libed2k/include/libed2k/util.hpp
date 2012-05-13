
#ifndef __LIBED2K_UTIL__
#define __LIBED2K_UTIL__

#include "libed2k/types.hpp"
#include "libed2k/constants.hpp"

namespace libed2k
{
    #define CHECK_BOM(size, x) ((size >= 3)  && (x[0] == (char)0xEF) && (x[1] == (char)0xBB) && (x[2] == (char)0xBF))

    template <typename A, typename B>
    inline A div_ceil(A a, B b)
    {
        return A((a + b - 1) / b);
    }

    template <typename A>
    inline A bits2bytes(A bits)
    {
        return div_ceil(bits, 8);
    }

    inline std::string int2ipstr(int ip)
    {
        std::stringstream ss;
        ss << (ip & 0xFF) << "." << ((ip >> 8) & 0xFF) << "." 
           << ((ip >> 16) & 0xFF) << "." << ((ip >> 24) & 0xFF);
        return ss.str();
    }

    extern std::string bitfield2string(const bitfield& bits);

    inline int round_up8(int v)
    {
        return ((v & 7) == 0) ? v : v + (8 - (v & 7));
    }

    extern std::pair<size_t, size_t> block_range(int piece, int block, size_t size);

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

    inline bool isLowId(boost::uint32_t nId)
    {
        return (nId < HIGHEST_LOWID_ED2K);
    }

    /**
      * @param vSrc     - incoming data buffer
      * @param vDst     - outgoing data buffer
      * @param nMaxSize - max size for incoming data
     */
    extern int inflate_gzip(const socket_buffer& vSrc, socket_buffer& vDst,
                            int nMaxSize);


    /**
      * truncate BOM header from UTF-8 strings
      *
     */
    extern std::string bom_filter(const std::string& s);

    /**
      * convert UTF-8 string to native codepage
     */
    extern std::string convert_to_native(const std::string& s);

    /**
      * make UTF-8 string from native string
     */
    extern std::string convert_from_native(const std::string& s);

}

#endif
