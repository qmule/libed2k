
#ifndef __FILE__HPP__
#define __FILE__HPP__

#include <string>
#include <vector>
#include "md4_hash.hpp"

namespace libed2k
{
    ///////////////////////////////////////////////////////////////////////////////
    // ED2K File Type
    //
    enum EED2KFileType
    {
        ED2KFT_ANY              = 0,
        ED2KFT_AUDIO            = 1,    // ED2K protocol value (eserver 17.6+)
        ED2KFT_VIDEO            = 2,    // ED2K protocol value (eserver 17.6+)
        ED2KFT_IMAGE            = 3,    // ED2K protocol value (eserver 17.6+)
        ED2KFT_PROGRAM          = 4,    // ED2K protocol value (eserver 17.6+)
        ED2KFT_DOCUMENT         = 5,    // ED2K protocol value (eserver 17.6+)
        ED2KFT_ARCHIVE          = 6,    // ED2K protocol value (eserver 17.6+)
        ED2KFT_CDIMAGE          = 7,    // ED2K protocol value (eserver 17.6+)
        ED2KFT_EMULECOLLECTION  = 8
    };

    // Media values for FT_FILETYPE
    const std::string ED2KFTSTR_AUDIO("Audio");
    const std::string ED2KFTSTR_VIDEO("Video");
    const std::string ED2KFTSTR_IMAGE("Image");
    const std::string ED2KFTSTR_DOCUMENT("Doc");
    const std::string ED2KFTSTR_PROGRAM("Pro");
    const std::string ED2KFTSTR_ARCHIVE("Arc");  // *Mule internal use only
    const std::string ED2KFTSTR_CDIMAGE("Iso");  // *Mule internal use only
    const std::string ED2KFTSTR_EMULECOLLECTION("EmuleCollection");

    // Additional media meta data tags from eDonkeyHybrid (note also the uppercase/lowercase)
    #define FT_ED2K_MEDIA_ARTIST    "Artist"    // <string>
    #define FT_ED2K_MEDIA_ALBUM     "Album"     // <string>
    #define FT_ED2K_MEDIA_TITLE     "Title"     // <string>
    #define FT_ED2K_MEDIA_LENGTH    "length"    // <string> !!!
    #define FT_ED2K_MEDIA_BITRATE   "bitrate"   // <uint32>
    #define FT_ED2K_MEDIA_CODEC     "codec"     // <string>


    EED2KFileType GetED2KFileTypeID(const std::string& strFileName);
    std::string GetED2KFileTypeSearchTerm(EED2KFileType iFileID);
    EED2KFileType GetED2KFileTypeSearchID(EED2KFileType iFileID);
    std::string GetFileTypeByName(const std::string& strFileName);

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
