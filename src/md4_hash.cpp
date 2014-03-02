#include "libed2k/md4_hash.hpp"
#include "libed2k/log.hpp"
#include "libed2k/hasher.hpp"

namespace libed2k
{
    const md4_hash md4_hash::terminal   = md4_hash::fromString("31D6CFE0D16AE931B73C59D7E0C089C0");
    const md4_hash md4_hash::libed2k    = md4_hash::fromString("31D6CFE0D14CE931B73C59D7E0C04BC0");
    const md4_hash md4_hash::emule      = md4_hash::fromString("31D6CFE0D10EE931B73C59D7E0C06FC0");
    const md4_hash md4_hash::invalid    = md4_hash::fromString("00000000000000000000000000000000");

    std::ostream& operator<< (std::ostream& stream, const md4_hash& hash)
    {
         stream << hash.toString();
         return stream;
    }

    /*static*/
    md4_hash md4_hash::fromHashset(const std::vector<md4_hash>& hashset)
    {
        md4_hash result;

        if (hashset.size() > 1)
        {
            MD4_CTX ctx;
            MD4_Init(&ctx);
            MD4_Update(&ctx, reinterpret_cast<const boost::uint8_t*>(&hashset[0]),
                    hashset.size() * MD4_DIGEST_LENGTH);
            MD4_Final(result.getContainer(), &ctx);
        }
        else if (hashset.size() == 1)
        {
            result = hashset[0];
        }

        return result;
    }

    void md4_hash::dump() const
    {
        DBG("md4_hash::dump " << toString().c_str());
    }
}
