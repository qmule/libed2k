
#ifndef __FILE__HPP__
#define __FILE__HPP__

#include <string>
#include <vector>
#include <queue>
#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include "error_code.hpp"
#include "md4_hash.hpp"
#include "packet_struct.hpp"
#include "session.hpp"

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

    const boost::uint8_t PR_VERYLOW     = 4;
    const boost::uint8_t PR_LOW         = 0;
    const boost::uint8_t PR_NORMAL      = 1;
    const boost::uint8_t PR_HIGH        = 2;
    const boost::uint8_t PR_VERYHIGH    = 3;
    const boost::uint8_t PR_AUTO        = 5;
    const boost::uint8_t PR_POWERSHARE  = 6;

    EED2KFileType GetED2KFileTypeID(const std::string& strFileName);
    std::string GetED2KFileTypeSearchTerm(EED2KFileType iFileID);
    EED2KFileType GetED2KFileTypeSearchID(EED2KFileType iFileID);
    std::string GetFileTypeByName(const std::string& strFileName);

    const unsigned int PIECE_COUNT_ALLOC = 20;

    // for future usage
    enum PreferencesDatFileVersions {
        PREFFILE_VERSION        = 0x14 //<<-- last change: reduced .dat, by using .ini
    };

    enum PartMetFileVersions {
        PARTFILE_VERSION            = 0xe0,
        PARTFILE_SPLITTEDVERSION    = 0xe1, // For edonkey part files importing.
        PARTFILE_VERSION_LARGEFILE  = 0xe2
    };

    enum CreditFileVersions {
        CREDITFILE_VERSION      = 0x12
    };

    enum CanceledFileListVersions {
        CANCELEDFILE_VERSION    = 0x21
    };

    // known.met file header
    const boost::uint8_t  MET_HEADER                  = 0x0E;
    const boost::uint8_t  MET_HEADER_WITH_LARGEFILES  = 0x0F;


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

    typedef container_holder<boost::uint16_t, std::vector<md4_hash> > hash_list;

    /**
      * simple known file entry structure
     */
    struct known_file_entry
    {
        boost::uint32_t             m_nLastChanged; //!< date last changed
        md4_hash                    m_hFile;        //!< file hash
        hash_list                   m_hash_list;
        tag_list<boost::uint32_t>   m_list;

        template<typename Archive>
        void serialize(Archive& ar)
        {
            ar & m_nLastChanged;
            ar & m_hFile;
            ar & m_hash_list;
            ar & m_list;
        }

        void dump() const;
    };

    typedef container_holder<boost::uint32_t, std::vector<known_file_entry> > known_file_list;

    /**
      * full known.met file content
     */
    struct known_file_collection
    {
        boost::uint8_t  m_nHeader;
        known_file_list m_known_file_list;

        template<typename Archive>
        void save(Archive& ar)
        {
            if (m_nHeader != MET_HEADER && m_nHeader != MET_HEADER_WITH_LARGEFILES)
            {
                // incorrect header
                throw libed2k_exception(errors::known_file_invalid_header);
            }

            ar & m_nHeader;
            ar & m_known_file_list;
        }

        template<typename Archive>
        void load(Archive& ar)
        {
            ar & m_nHeader;

            if (m_nHeader != MET_HEADER && m_nHeader != MET_HEADER_WITH_LARGEFILES)
            {
                // incorrect header
                throw libed2k_exception(errors::known_file_invalid_header);
            }

            ar & m_known_file_list;
        }


        void dump() const;

        LIBED2K_SERIALIZATION_SPLIT_MEMBER()
    };

    /**
      * simple monitor object
     */
    template <typename Data>
    class monitor_order : boost::noncopyable
    {
    public:
        void push(const Data& data)
        {
            boost::mutex::scoped_lock lock(m_monitorMutex);
            m_queue.push(data);
            m_signal.notify_one();
        }

        void cancel()
        {
            boost::mutex::scoped_lock lock(m_monitorMutex);
            std::queue<std::string> empty;
            std::swap(m_queue, empty );
            m_signal.notify_one();
        }

        Data popWait()
        {
            boost::mutex::scoped_lock lock(m_monitorMutex);

            if(m_queue.empty())
            {
                m_signal.wait(lock);
            }

            // we have received exit signal
            if (m_queue.empty())
            {
                throw libed2k_exception(errors::no_error);
            }

            Data temp(m_queue.front());
            m_queue.pop();
            return temp;
        }

    private:
        std::queue<Data> m_queue;
        boost::mutex m_monitorMutex;
        boost::condition m_signal;
    };

    namespace aux { class session_impl; }

    class file_monitor
    {
    public:
        file_monitor(add_transfer_handler handler);

        /**
          * start monitor thread
         */
        void start();

        /**
          * cancel all current works and wait thread exit
         */
        void stop();

        void operator()();

        monitor_order<std::string>  m_order;

    private:
        volatile bool           m_bCancel;
        add_transfer_handler    m_add_transfer;
        boost::shared_ptr<boost::thread> m_thread;
        boost::mutex m_mutex;
    };

}

#endif //__FILE__HPP__
