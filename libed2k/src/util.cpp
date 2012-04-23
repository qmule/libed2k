
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <cryptopp/md4.h>

#include "util.hpp"

namespace libed2k 
{

md4_hash hash_md4(const std::string& str) {
    CryptoPP::Weak::MD4 hash;
    md4_hash::md4hash_container digest;

    hash.Update((const unsigned char*)str.c_str(), str.size());
    hash.Final(digest);
    return md4_hash(digest);
}

}
