
#include "md4_hash.hpp"

namespace libed2k{



const md4_hash::md4hash_container md4_hash::m_emptyMD4Hash = 
{'\x31', '\xD6', '\xCF', '\xE0', '\xD1', '\x6A', '\xE9', '\x31' , '\xB7', '\x3C', '\x59', '\xD7', '\xE0', '\xC0', '\x89', '\xC0' };

const md4_hash md4_hash::terminal = md4_hash("31D6CFE0D16AE931B73C59D7E0C089C0");

std::ostream& operator<< (std::ostream& stream, const md4_hash& hash)
{
    stream << hash.toString();
    return stream;
}

}
