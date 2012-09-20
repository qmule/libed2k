
#ifndef __LIBED2K_UTIL__
#define __LIBED2K_UTIL__

#include "libed2k/bitfield.hpp"
#include "libed2k/constants.hpp"
#include "libed2k/ptime.hpp"

namespace libed2k
{
    #define CHECK_BOM(size, x) ((size >= 3)  && (x[0] == (char)0xEF) && (x[1] == (char)0xBB) && (x[2] == (char)0xBF))

    template <typename A, typename B>
    inline A div_ceil(A a, B b)
    {
        return A((a + b - 1)/b);
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

    inline size_t piece_count(fsize_t fsize)
    {
        return static_cast<size_t>(div_ceil(fsize, PIECE_SIZE));
    }

    extern std::pair<fsize_t, fsize_t> block_range(int piece, int block, fsize_t size);

    template<typename Coll1, typename Coll2>
    void appendAll(Coll1& to, const Coll2& from)
    {
        std::copy(from.begin(), from.end(), std::inserter(to, to.end()));
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
        duration_timer(const time_duration& duration, const ptime& last_tick = time_now());
        bool expires();
        const time_duration& tick_interval() const { return m_tick_interval; }
    private:
        time_duration m_duration;
        ptime m_last_tick;
        time_duration m_tick_interval;
    };

    // set of semi open intervals: [a1, b1), [a2, b2), ...
    template <typename T>
    class range
    {
    public:
        typedef typename std::pair<T,T> segment;
        typedef typename std::vector<segment> segments;

        range(const segment& seg)
        {
            m_segments.push_back(seg);
        }

        range<T>& operator-=(const segment& seg)
        {
            check(seg);
            segments res;
            for (typename segments::iterator i = m_segments.begin(); i != m_segments.end(); ++i)
                appendAll(res, sub(*i, seg));
            m_segments = res;

            return *this;
        }

        bool empty() const { return m_segments.empty(); }

    private:
        segments m_segments;

        void check(const segment& seg) { assert(seg.first <= seg.second); }

        segments sub(const segment& seg1, const segment& seg2)
        {
            check(seg1);
            check(seg2);

            segments res;

            // [   seg1    )
            //   [ seg2 )    -> [ )   [ )
            if (seg1.first < seg2.first && seg1.second > seg2.second)
            {
                res.push_back(std::make_pair(seg1.first, seg2.first));
                res.push_back(std::make_pair(seg2.second, seg1.second));
            }
            // [ seg1 )
            //          [ seg2 ) -> [   )
            else if (seg1.second <= seg2.first || seg2.second <= seg1.first)
            {
                res.push_back(seg1);
            }
            // [ seg1 )
            //    [ seg2 ) -> [  )
            else if (seg2.first > seg1.first && seg2.first < seg1.second)
            {
                res.push_back(std::make_pair(seg1.first, seg2.first));
            }
            //     [ seg1 )
            // [ seg2 )     -> [  )
            else if (seg2.second > seg1.first && seg2.second < seg1.second)
            {
                res.push_back(std::make_pair(seg2.second, seg1.second));
            }

            return res;
        }
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

#if 0
    /**
      * @param vSrc     - incoming data buffer
      * @param vDst     - outgoing data buffer
      * @param nMaxSize - max size for incoming data
     */
    extern int inflate_gzip(const socket_buffer& vSrc, socket_buffer& vDst,
                            int nMaxSize);
#endif

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
