
#ifndef __FILE__HPP__
#define __FILE__HPP__

#include <string>
#include <vector>
#include "md4_hash.hpp"

namespace libed2k
{
    const unsigned int PIECE_COUNT_ALLOC = 20;

    class known_file
    {
    public:
        // TODO: should use fs::path as parameter
        known_file(const std::string& strFilename);

        /**
          * calculate file parameters
         */
        void init();

        const md4_hash& getFileHash() const;
        const md4_hash& getPieceHash(size_t nPart) const;
        size_t          getPiecesCount() const;
    private:
        std::string             m_strFilename;
        std::vector<md4_hash>   m_vHash;
        md4_hash                m_hFile;
    };
}

#endif //__FILE__HPP__
