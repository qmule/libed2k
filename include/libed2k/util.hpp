
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

    /**
      * this function convert network by order unsigned int IP address
      * to network by order dot-notation string
     */
    inline std::string int2ipstr(unsigned int ip)
    {
        std::stringstream ss;
        ss << (ip & 0xFF) << "." << ((ip >> 8) & 0xFF) << "." 
           << ((ip >> 16) & 0xFF) << "." << ((ip >> 24) & 0xFF);
        return ss.str();
    }

    /**
      * convert host byte order asio address uint to network order int ip
     */
    inline boost::uint32_t address2int(const ip::address& addr)
    {
        assert(addr.is_v4());
        return htonl(addr.to_v4().to_ulong());
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
            return m_it.operator->();
        }

        cyclic_iterator& operator++() {
            ++m_it;
            validate();
            return *this;
        }

        cyclic_iterator& inc() {
            ++m_it;
            return *this;
        }

        operator I&() { return m_it; }
        operator const I&() const { return m_it; }

        void validate() {
            if (m_it == m_cont.end()) m_it = m_cont.begin();
        }
    };

    class duration_timer
    {
    public:
        duration_timer(const time::time_duration& duration, const ptime& last_tick = time_now());
        bool expires();
        const time::time_duration& tick_interval() const { return m_tick_interval; }
    private:
        time::time_duration m_duration;
        ptime m_last_tick;
        time::time_duration m_tick_interval;
    };

    inline bool isLowId(boost::uint32_t nId)
    {
        return (nId < HIGHEST_LOWID_ED2K);
    }

    template<typename T1, typename T2>
    inline const T2& take_second(const std::pair<T1, T2> &pair)
    {
        return pair.second;
    }

    template<typename T1, typename T2>
    inline const T1& take_first(const std::pair<T1, T2> &pair)
    {
        return pair.first;
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
