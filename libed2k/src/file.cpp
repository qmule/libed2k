#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem.hpp>
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <cryptopp/md4.h>
#include <cassert>
#include "constants.hpp"
#include "log.hpp"
#include "md4_hash.hpp"
#include "types.hpp"
#include "file.hpp"

namespace libed2k
{

    known_file::known_file(const std::string& strFilename) : m_strFilename(strFilename)
    {
    }

    const md4_hash& known_file::getFileHash() const
    {
        assert(!m_vHash.empty());
        return (m_hFile);
    }

    const md4_hash& known_file::getPieceHash(size_t nPart) const
    {
        assert(!m_vHash.empty());

        if (nPart >= m_vHash.size())
        {
            // index error
        }

        return (m_vHash[nPart]);
    }


    size_t known_file::getPiecesCount() const
    {
        assert(!m_vHash.empty());
        return (m_vHash.size());
    }

    void known_file::init()
    {
        fs::path p(m_strFilename);

        if (!fs::exists(p) || !fs::is_regular_file(p))
        {
            throw libed2k_exception(errors::file_unavaliable);
        }

        bool    bPartial = false; // check last part in file not full
        uintmax_t nFileSize = boost::filesystem::file_size(p);

        if (nFileSize == 0)
        {
            throw libed2k_exception(errors::filesize_is_zero);
            return;
        }

        int nPartCount  = nFileSize / PIECE_SIZE + 1;
        m_vHash.reserve(nPartCount);

        bio::mapped_file_params mf_param;
        mf_param.flags  = bio::mapped_file_base::readonly;
        mf_param.path   = m_strFilename;
        mf_param.length = 0;


        bio::mapped_file_source fsource;
        DBG("System alignment: " << bio::mapped_file::alignment());

        uintmax_t nCurrentOffset = 0;
        CryptoPP::Weak1::MD4 md4_hasher;

        while(nCurrentOffset < nFileSize)
        {
            uintmax_t nMapPosition = (nCurrentOffset / bio::mapped_file::alignment()) * bio::mapped_file::alignment();    // calculate appropriate mapping start position
            uintmax_t nDataCorrection = nCurrentOffset - nMapPosition;                                          // offset to data start

            // calculate map size
            uintmax_t nMapLength = PIECE_SIZE*PIECE_COUNT_ALLOC;

            // correct map length
            if (nMapLength > (nFileSize - nCurrentOffset))
            {
                nMapLength = nFileSize - nCurrentOffset + nDataCorrection;
            }
            else
            {
                nMapLength += nDataCorrection;
            }

            mf_param.offset = nMapPosition;
            mf_param.length = nMapLength;
            fsource.open(mf_param);

            uintmax_t nLocalOffset = nDataCorrection; // start from data correction offset

            DBG("Map position   : " << nMapPosition);
            DBG("Data correction: " << nDataCorrection);
            DBG("Map length     : " << nMapLength);
            DBG("Map piece count: " << nMapLength / PIECE_SIZE);

            while(nLocalOffset < nMapLength)
            {
                // calculate current part size size
                uintmax_t nLength = PIECE_SIZE;

                if (PIECE_SIZE > nMapLength - nLocalOffset)
                {
                    nLength = nMapLength-nLocalOffset;
                    bPartial = true;
                }

                DBG("         local piece: " << nLength);

                libed2k::md4_hash hash;
                md4_hasher.CalculateDigest(hash.getContainer(), reinterpret_cast<const unsigned char*>(fsource.data() + nLocalOffset), nLength);
                m_vHash.push_back(hash);
                // generate hash
                nLocalOffset    += nLength;
                nCurrentOffset  += nLength;
            }

            fsource.close();

        }

        // when we don't have last partial piece - add special hash
        if (!bPartial)
        {
            m_vHash.push_back(md4_hash("31D6CFE0D16AE931B73C59D7E0C089C0"));
        }

        if (m_vHash.size() > 1)
        {
            md4_hasher.CalculateDigest(m_hFile.getContainer(), reinterpret_cast<const unsigned char*>(&m_vHash[0]), m_vHash.size()*libed2k::MD4_HASH_SIZE);
        }
        else
        {
            m_hFile = m_vHash[0];
        }

    }

}
