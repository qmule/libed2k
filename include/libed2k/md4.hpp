/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security,
 * Inc. MD4 Message-Digest Algorithm.
 *
 * Written by Solar Designer <solar@openwall.com> in 2001, and placed in
 * the public domain.  See md4.c for more information.
 */

#ifndef __MD4_H
#define __MD4_H

#include "libed2k/size_type.hpp"

namespace libed2k
{
    const size_t MD4_HASH_SIZE = 128/8;

    struct md4_context
    {
        boost::uint32_t lo, hi;
        boost::uint32_t a, b, c, d;
        unsigned char buffer[64];
        boost::uint32_t block[MD4_HASH_SIZE];
    };

    void md4_init(struct md4_context *ctx);
    void md4_update(struct md4_context *ctx, const unsigned char *data, size_t size);
    void md4_final(struct md4_context *ctx, unsigned char result[MD4_HASH_SIZE]);
}

#endif
