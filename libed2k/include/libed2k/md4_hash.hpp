
#ifndef __LIBED2K_MD4_HASH__
#define __LIBED2K_MD4_HASH__

#include <string>
#include <sstream>
#include <string.h>
#include <algorithm>
#include <typeinfo>
#include <vector>

#include <boost/cstdint.hpp>
#include <boost/assert.hpp>
#include <boost/optional.hpp>

#include <libtorrent/bitfield.hpp>
#include <libtorrent/escape_string.hpp>

#include "libed2k/log.hpp"
#include "libed2k/archive.hpp"
#include "libed2k/error_code.hpp"
#include "libed2k/types.hpp"

namespace libed2k{

    const size_t MD4_HASH_SIZE = 16;

    /**
      * this class simple container for hash array
     */
    class md4_hash
    {
    public:
        friend class archive::access;
        typedef boost::uint8_t md4hash_container[MD4_HASH_SIZE];
        static const md4hash_container m_emptyMD4Hash;
        static const md4_hash terminal;

        enum { hash_size = MD4_HASH_SIZE };

    	md4_hash()
    	{
    	    clear();
    	}

    	md4_hash(const std::vector<boost::uint8_t>& vHash)
    	{
            size_t nSize = (vHash.size()> MD4_HASH_SIZE)?MD4_HASH_SIZE:vHash.size();

    		for (size_t i = 0; i < nSize; i++)
    		{
    			m_hash[i] = vHash[i];
    		}
    	}

    	md4_hash(const md4hash_container& container)
    	{
    		memcpy(m_hash, container, MD4_HASH_SIZE);
    	}

        bool defined() const
        {
            int sum = 0;
            for (size_t i = 0; i < MD4_HASH_SIZE; ++i)
                sum |= m_hash[i];
            return sum != 0;
        }

    	unsigned char* getContainer()
    	{
    	    return &m_hash[0];
    	}

    	bool operator==(const md4_hash& hash) const
        {
    	    return (memcmp(m_hash, hash.m_hash, MD4_HASH_SIZE) == 0);
        }

    	bool operator<(const md4_hash& hash) const
    	{
    	    return (memcmp(m_hash, hash.m_hash, MD4_HASH_SIZE) < 0);
    	}

    	bool operator>(const md4_hash& hash) const
    	{
    	     return (memcmp(m_hash, hash.m_hash, MD4_HASH_SIZE) > 0);
    	}

    	void clear()
    	{
    	    memset(m_hash, 0, MD4_HASH_SIZE);
    	}

    	static md4_hash fromString(const std::string& strHash)
    	{
    	    BOOST_ASSERT(strHash.size() == MD4_HASH_SIZE*2);

    	    md4_hash hash;

    	    if (!libtorrent::from_hex(strHash.c_str(), MD4_HASH_SIZE*2, (char*)hash.m_hash))
    	    {
    	        return (md4_hash());
    	    }

    	    return (hash);
    	}

    	std::string toString() const
    	{
    	    std::stringstream ss;

    	    for (size_t i = 0; i < MD4_HASH_SIZE; i++)
    	    {
    	        ss << std::uppercase << std::hex << (m_hash[i] >> 4) << (m_hash[i] & 0x0F);
    	    }

    	    return (ss.str());
    	}

    	boost::uint8_t operator[](size_t n) const
    	{
    	    if (n >= MD4_HASH_SIZE)
            {
                throw libed2k_exception(errors::md4_hash_index_error);
            }

    	    return (m_hash[n]);
    	}

    	boost::uint8_t& operator[](size_t n)
        {
    	    if (n >= MD4_HASH_SIZE)
            {
                throw libed2k_exception(errors::md4_hash_index_error);
            }
    	    return (m_hash[n]);
        }

    	template<typename Archive>
    	void serialize(Archive& ar)
    	{
    	    for (size_t n = 0; n < sizeof(md4hash_container); n++)
    	    {
    	        ar & m_hash[n];
    	    }
    	}

    	void dump() const
    	{
    	    DBG("md4_hash::dump " << toString().c_str());
    	}

    	/**
    	  * for using in logger output
    	 */
    	friend std::ostream& operator<< (std::ostream& stream, const md4_hash& hash);
    private:
    	md4hash_container   m_hash;
    };

    class hash_set
    {
    public:
        hash_set(): m_has_terminal(false)
        {}

        const bitfield& pieces() const { return m_pieces; }
        const std::vector<md4_hash>& hashes() const { return m_hashes; }
        std::vector<md4_hash> all_hashes() const
        {
            std::vector<md4_hash> res;
            size_t h = 0;
            for(size_t p = 0; p < m_pieces.size(); ++p)
                res.push_back(m_pieces[p] ? m_hashes[h++] : md4_hash());

            if (m_has_terminal) res.push_back(md4_hash::terminal);

            return res;
        }
        boost::optional<const md4_hash&> hash(size_t piece_index) const
        {
            size_t h = 0;
            for(size_t p = 0; p < piece_index; ++p)
                if (m_pieces[p]) ++h;
            return m_pieces[piece_index] ?
                boost::optional<const md4_hash&>(m_hashes[h]):
                boost::optional<const md4_hash&>();
        }

        void pieces(const bitfield& ps) { m_pieces = ps; }
        void hashes(const std::vector<md4_hash>& hs) { m_hashes = hs; }
        void append(const md4_hash& hash)
        {
            size_t i = m_pieces.size();
            m_pieces.resize(i + 1);
            m_pieces.set_bit(i);
            m_hashes.push_back(hash);
        }
        void hash(size_t piece_index, const md4_hash& _hash)
        {
            std::vector<md4_hash>::iterator pos = m_hashes.begin();
            for(size_t p = 0; p < piece_index; ++p) if (m_pieces[p]) ++pos;
            if (m_pieces[piece_index]) *pos = _hash;
            else m_hashes.insert(pos, _hash);
        }
        void set_terminal() { m_has_terminal = true; }
        void all_hashes(const std::vector<md4_hash>& hs)
        {
            size_t nPieces;

            BOOST_ASSERT(!hs.empty());

            if (*hs.rbegin() == md4_hash::terminal)
            {
                set_terminal();
                nPieces = hs.size() - 1;
            }
            else
            {
                nPieces = hs.size();
            }

            m_pieces.resize(nPieces);
            m_pieces.clear_all();
            for (size_t i = 0; i != nPieces; ++i)
            {
                if (hs[i].defined())
                {
                    m_pieces.set_bit(i);
                    m_hashes.push_back(hs[i]);
                }
            }
        }
        void reset(size_t pieces)
        {
            m_pieces.resize(pieces);
            m_pieces.clear_all();
            m_hashes.clear();
        }

    private:
        bitfield m_pieces;
        std::vector<md4_hash> m_hashes;
        bool m_has_terminal;
    };
}



#endif
