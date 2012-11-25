#include <iostream>
#include <fstream>

#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem.hpp>

#include <cryptopp/md4.h>
#include "libed2k/constants.hpp"
#include "libed2k/log.hpp"
#include "libed2k/md4_hash.hpp"

const size_t nPieceCount = 10;

using namespace libed2k;

void generate_file(size_t nSize, const char* pchFilename)
{
    std::ofstream of(pchFilename, std::ios_base::binary);

    if (of)
    {
        // generate small file
        for (int i = 0; i < nSize; i++)
        {
            of << 'X';
        }
    }
}

int main(int argc, char* argv[])
{
    LOGGER_INIT(LOG_ALL) 
#if 0

    if (argc < 2)
    {
        std::cerr << "set filename for hashing" << std::endl;
        return 1;
    }

    generate_file(100, "./test1.bin");
    generate_file(libed2k::PIECE_SIZE, "./test2.bin");
    generate_file(libed2k::PIECE_SIZE+1, "./test3.bin");
    generate_file(libed2k::PIECE_SIZE*4, "./test4.bin");
    generate_file(libed2k::PIECE_SIZE+4566, "./test5.bin");


    namespace bio = boost::iostreams;
    std::vector<libed2k::md4_hash> vH;
    // calculate hash
    bool    bPartial = false;
    uintmax_t nFileSize = boost::filesystem::file_size(argv[1]);
    int nPartCount  = nFileSize / PIECE_SIZE + 1;
    vH.reserve(nPartCount);
    int nAlignment = bio::mapped_file::alignment();

    bio::mapped_file_params mf_param;

    mf_param.flags  = bio::mapped_file_base::readonly; //std::ios_base::openmode::
    mf_param.path   = argv[1];
    mf_param.length = 0;


    bio::mapped_file_source fsource;
    DBG("System alignment: " << bio::mapped_file::alignment());
    uintmax_t nCurrentOffset = 0;

    CryptoPP::Weak1::MD4 md4_hasher;

    while(nCurrentOffset < nFileSize)
    {
        uintmax_t nMapPosition = (nCurrentOffset / nAlignment) * nAlignment;    // calculate appropriate mapping start position
        uintmax_t nDataCorrection = nCurrentOffset - nMapPosition;                                          // offset to data start

        // calculate map size
        uintmax_t nMapLength = PIECE_SIZE*nPieceCount;

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
            vH.push_back(hash);
            // generate hash
            nLocalOffset    += nLength;
            nCurrentOffset  += nLength;
        }

        fsource.close();

    }

    if (bPartial)
    {
        std::cout << "we have partial piece" << std::endl;
    }
    else
    {
        vH.push_back(md4_hash::fromString("31D6CFE0D16AE931B73C59D7E0C089C0"));
    }

    for(int n = 0; n < vH.size(); n++)
    {
        DBG("HASH Part: " << vH[n].toString());
    }

    libed2k::md4_hash hResult;
    if (vH.size() > 1)
    {
        md4_hasher.CalculateDigest(hResult.getContainer(), reinterpret_cast<const unsigned char*>(&vH[0]), vH.size()*libed2k::MD4_HASH_SIZE);
    }
    else
    {
        hResult = vH[0];
    }

    DBG("Result hash: " << hResult.toString());
#endif
    return 0;
}
