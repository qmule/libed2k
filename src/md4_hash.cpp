
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <cryptopp/md4.h>

#include "libed2k/md4_hash.hpp"

namespace libed2k
{
    const md4_hash md4_hash::terminal   = md4_hash::fromString("31D6CFE0D16AE931B73C59D7E0C089C0");
    const md4_hash md4_hash::libed2k    = md4_hash::fromString("31D6CFE0D14CE931B73C59D7E0C04BC0");
    const md4_hash md4_hash::emule      = md4_hash::fromString("31D6CFE0D10EE931B73C59D7E0C06FC0");

    std::ostream& operator<< (std::ostream& stream, const md4_hash& hash)
    {
        stream << hash.toString();
        return stream;
    }

    /*static*/
    md4_hash md4_hash::fromHashset(const std::vector<md4_hash>& hashset)
    {
        md4_hash result;
        CryptoPP::Weak1::MD4 md4_hasher;

        if (hashset.size() > 1)
        {
            md4_hasher.CalculateDigest(
                result.getContainer(),
                reinterpret_cast<const unsigned char*>(&hashset[0]),
                hashset.size() * MD4_HASH_SIZE);
        }
        else if (hashset.size() == 1)
        {
            result = hashset[0];
        }

        return result;
    }
}
