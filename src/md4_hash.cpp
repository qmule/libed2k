
#include "libed2k/md4_hash.hpp"

namespace libed2k
{
    const md4_hash md4_hash::terminal   = md4_hash::fromString("31D6CFE0D16AE931B73C59D7E0C089C0");
    const md4_hash md4_hash::libed2k    = md4_hash::fromString("31D6CFE0D14CE931B73C59D7E0C04BC0");

    std::ostream& operator<< (std::ostream& stream, const md4_hash& hash)
    {
        stream << hash.toString();
        return stream;
    }

}
