
#ifndef __FILE__HPP__
#define __FILE__HPP__

#include <string>
#include <vector>
#include <queue>
#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include "libed2k/error_code.hpp"
#include "libed2k/md4_hash.hpp"
#include "libed2k/packet_struct.hpp"
#include "libed2k/session.hpp"

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
    const std::string ED2KFTSTR_FOLDER("Folder"); // Value for eD2K tag FT_FILETYPE
    const std::string ED2KFTSTR_USER("User"); // eMule internal use only

    // Additional media meta data tags from eDonkeyHybrid (note also the uppercase/lowercase)
    const std::string FT_ED2K_MEDIA_ARTIST("Artist");    // <string>
    const std::string FT_ED2K_MEDIA_ALBUM(     "Album");     // <string>
    const std::string FT_ED2K_MEDIA_TITLE(     "Title");     // <string>
    const std::string FT_ED2K_MEDIA_LENGTH(    "length");    // <string> !!!
    const std::string FT_ED2K_MEDIA_BITRATE(   "bitrate");   // <uint32>
    const std::string FT_ED2K_MEDIA_CODEC(     "codec");    // <string>
    // ?
    #define TAG_NSENT               "# Sent"
    #define TAG_ONIP                "ip"
    #define TAG_ONPORT              "port"



    const boost::uint8_t PR_VERYLOW     = 4;
    const boost::uint8_t PR_LOW         = 0;
    const boost::uint8_t PR_NORMAL      = 1;
    const boost::uint8_t PR_HIGH        = 2;
    const boost::uint8_t PR_VERYHIGH    = 3;
    const boost::uint8_t PR_AUTO        = 5;
    const boost::uint8_t PR_POWERSHARE  = 6;

    extern EED2KFileType GetED2KFileTypeID(const std::string& strFileName);
    extern std::string GetED2KFileTypeSearchTerm(EED2KFileType iFileID);
    extern EED2KFileType GetED2KFileTypeSearchID(EED2KFileType iFileID);
    extern std::string GetFileTypeByName(const std::string& strFileName);

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
        const std::vector<md4_hash>& getPieceHashes() const { return m_vHash; }
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

        known_file_entry();

        known_file_entry(const md4_hash& hFile,
                            const std::vector<md4_hash>& hSet,
                            const fs::path& p,
                            size_t  nFilesize,
                            boost::uint32_t nAccepted,
                            boost::uint32_t nRequested,
                            boost::uint64_t nTransferred,
                            boost::uint8_t  nPriority);

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

    typedef container_holder<boost::uint32_t, std::deque<known_file_entry> > known_file_list;

    /**
      * full known.met file content
     */
    struct known_file_collection
    {
        boost::uint8_t  m_nHeader;
        known_file_list m_known_file_list;

        known_file_collection();

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
        monitor_order()
        {
            m_cancelled = false;
        }

        void push(const Data& data)
        {
            boost::mutex::scoped_lock lock(m_monitorMutex);
            m_queue.push(data);
            m_cancelled = false;
            m_signal.notify_one();
        }

        void cancel()
        {
            DBG("monitor:: cancel");
            boost::mutex::scoped_lock lock(m_monitorMutex);
            std::queue<fs::path> empty;
            std::swap(m_queue, empty );
            m_cancelled = true;
            m_signal.notify_one();
            DBG("monitor:: completed");
        }

        Data popWait()
        {
            boost::mutex::scoped_lock lock(m_monitorMutex);

            if (m_cancelled)
            {
                throw libed2k_exception(errors::no_error);
            }

            if(m_queue.empty())
            {
                DBG("popWait: before signal");
                m_signal.wait(lock);
            }

            DBG("popWait: after signal");

            if (m_queue.empty())
            {
                // we have exit signal
                throw libed2k_exception(errors::no_error);
            }

            Data temp(m_queue.front());
            m_queue.pop();
            return temp;
        }

    private:
        bool                m_cancelled;
        std::queue<Data>    m_queue;
        boost::mutex        m_monitorMutex;
        boost::condition    m_signal;
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

        monitor_order<fs::path>  m_order;

    private:
        volatile bool           m_bCancel;
        add_transfer_handler    m_add_transfer;
        boost::shared_ptr<boost::thread> m_thread;
        boost::mutex m_mutex;
    };

    /**
      * rule policy
     */
    class rule
    {
    public:
        enum rule_type
        {
            rt_plus,
            rt_minus,
            rt_asterisk
        };

        rule(rule_type rt, const std::string& strPath);
        ~rule();
        const rule* get_parent() const;
        const std::string get_filename() const;
        const fs::path& get_path() const;
        rule_type get_type() const;
        rule* add_sub_rule(rule_type rt, const std::string& strPath);

        /**
          * when appropriate rule was found - return it,
          * otherwise return NULL
         */
        rule* match(const fs::path& path);
    private:
        rule* append_rule(rule_type rt, const fs::path& path);
        rule(rule_type rt, const fs::path& path, rule* parent);
        rule_type           m_type;
        rule*               m_parent;
        fs::path            m_path;
        std::deque<rule*>   m_sub_rules;
    };

    struct emule_collection_entry
    {
        emule_collection_entry() : m_filename(""), m_filesize(0) {}
        emule_collection_entry(const std::string& strFilename, boost::uint64_t nFilesize, const md4_hash& hash) :
            m_filename(strFilename), m_filesize(nFilesize), m_filehash(hash) {}
        std::string     m_filename;
        boost::uint64_t m_filesize;
        md4_hash        m_filehash;
    };

    struct emule_collection
    {
        static emule_collection fromFile(const std::string& strFilename);
        void save(const std::string& strFilename, bool binary = false);
        bool add_file(const std::string& strFilename, boost::uint64_t nFilesize, const std::string& strFilehash);
        bool add_link(const std::string& strLink);
        const std::string get_ed2k_link(size_t nIndex);
        std::deque<emule_collection_entry> m_files;
    };

    /**
      * structure for save/load binary emulecollection files
     */
    struct emule_binary_collection
    {
        boost::uint32_t m_nVersion;
        tag_list<boost::uint32_t>   m_list;
        container_holder<boost::uint32_t, std::vector<tag_list<boost::uint32_t> > >   m_files;

        template<typename Archive>
        void serialize(Archive& ar)
        {
            ar & m_nVersion;
            ar & m_list;
            ar & m_files;
        }

        bool operator==(const emule_binary_collection& ec) const;
        void dump() const;
    };

    /**
      * class for manager one collection entry
     */
    class collection
    {
    public:
        collection();
        void add_file(const std::string& strFilename, size_t nFilesize, const md4_hash& hFile);

        void load(const std::string& strWorkspace);
        void save(const std::string& strWorkspace);
        bool is_obsolete() const;
        void set_obosolete();
        void set_name(const std::string& strName);
        bool unnamed() const;
        bool operator==(const collection& c) const;
    private:
        std::string         m_strName;
        bool                m_obsolete;
        bool                m_saved;
        emule_binary_collection    m_content;
    };

    /**
      * class to manage mule collections
      *
     */
    class collection_manager
    {
    public:
        void load(const std::string& strWorkspace);
        void set_collection(const collection& c);
    private:
        std::deque<collection> m_collections;
    };

}

#endif //__FILE__HPP__
