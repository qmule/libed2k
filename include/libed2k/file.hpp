
#ifndef __FILE__HPP__
#define __FILE__HPP__

#include <string>
#include <vector>
#include <deque>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>

#include "libed2k/config.hpp"
#include "libed2k/error_code.hpp"
#include "libed2k/size_type.hpp"
#include "libed2k/alert.hpp"
#include "libed2k/filesystem.hpp"
#include "libed2k/md4_hash.hpp"
#include "libed2k/packet_struct.hpp"
#include "libed2k/alert_types.hpp"

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

    /**
      * this function work correctly only with lower case file extensions!
     */
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

    // one byte header for met files
    struct met_file_header
    {
        boost::uint8_t m_header;
        met_file_header() : m_header(MET_HEADER_WITH_LARGEFILES){}

        template<typename Archive>
        void serialize(Archive& ar)
        {
            ar & m_header;
            if (m_header != MET_HEADER && m_header != MET_HEADER_WITH_LARGEFILES)
                throw libed2k_exception(errors::met_file_invalid_header_byte);
        }
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
                            const std::string& filename,
                            size_type  nFilesize,
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
        met_file_header     m_header;
        known_file_list     m_known_file_list;

        known_file_collection();
        add_transfer_params extract_transfer_params(time_t, const std::string&);

        template<typename Archive>
        void serialize(Archive& ar)
        {
            ar & m_header;
            ar & m_known_file_list;
        }

        void dump() const;
    };

    /**
      * structure for save transfer resume data and additional info like hash, filepath, filesize
     */
    struct transfer_resume_data
    {
        md4_hash    m_hash;     //!< transfer hash
        container_holder<boost::uint16_t, std::string> m_filepath; //!< utf-8 file path
        size_type     m_filesize;
        tag_list<boost::uint8_t>    m_fast_resume_data;
        transfer_resume_data();
        transfer_resume_data(const md4_hash& hash,
                const std::string& save_path,
                const std::string& filename,
                size_type size,
                const std::vector<char>& fr_data);

        template<typename Archive>
        void serialize(Archive& ar)
        {
            ar & m_hash;
            ar & m_filepath;
            ar & m_filesize;
            ar & m_fast_resume_data;
        }
    };

    struct file2atp : public std::binary_function<const std::string&, bool&, std::pair<add_transfer_params, error_code> >
    {
        std::pair<add_transfer_params, error_code> operator()(const std::string&, const bool&);
    };

    class transfer_params_maker
    {
    public:
        transfer_params_maker(alert_manager& am, const std::string& known_filepath);
        virtual ~transfer_params_maker();
        bool start();
        void stop();
        void operator()();

        size_t order_size();
        std::string current_filepath();

        /**
          * @param filepath in UTF-8
         */
        void make_transfer_params(const std::string& filepath);
        void cancel_transfer_params(const std::string& filepath);
    protected:
        virtual void process_item();
        alert_manager&      m_am;
        mutable bool        m_abort;                //!< cancel thread
        mutable bool        m_abort_current;       //!< cancel one file
        std::string         m_current_filepath;     //!< current file path
    private:
        std::string m_known_filepath;
        known_file_collection m_kfc;
        boost::shared_ptr<boost::thread> m_thread;

        boost::mutex m_mutex;
        std::deque<std::string>    m_order;
        std::queue<std::string>    m_cancel_order;  //!< order for store signals to cancel after
        boost::condition           m_condition;

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
      * one file collection entry
     */
    struct emule_collection_entry
    {
        emule_collection_entry() : m_filename(""), m_filesize(0)
        {
        }

        emule_collection_entry(const std::string& strFilename, size_type nFilesize, const md4_hash& hash) :
            m_filename(strFilename), m_filesize(nFilesize), m_filehash(hash) {}

        bool operator==(const emule_collection_entry& ce) const
        {
            return (m_filename == ce.m_filename &&
                    m_filesize == ce.m_filesize &&
                    m_filehash == ce.m_filehash);
        }

        bool defined() const
        {
            return (!m_filename.empty() && m_filesize != 0 && m_filehash.defined());
        }

        std::string     m_filename;
        size_type       m_filesize;
        md4_hash        m_filehash;
    };

    /**
      * files collection
     */
    struct emule_collection
    {
        /**
          * restore collection from file
         */
        static emule_collection fromFile(const std::string& strFilename);

        /**
          * generate ed2k link from collection item
         */
        static std::string toLink(const std::string& strFilename, size_type nFilesize, const md4_hash& hFile, bool uencode = false);
        static emule_collection_entry fromLink(const std::string& strLink);

        bool save(const std::string& strFilename, bool binary = false);

        /**
          * add known file
         */
        bool add_file(const std::string& strFilename, size_type nFilesize, const std::string& strFilehash);
        bool add_link(const std::string& strLink);

        const std::string get_ed2k_link(size_t nIndex);
        bool operator==(const emule_collection& ecoll) const;
        std::string                         m_name;
        std::deque<emule_collection_entry>  m_files;
    };

    // server identifier in server.met file
    struct server_met_entry
    {
        net_identifier              m_network_point;
        tag_list<boost::uint32_t>   m_list;

        template<typename Archive>
        void serialize(Archive& ar)
        {
            ar & m_network_point;
            ar & m_list;
        }

        bool operator==(const server_met_entry& e) const
        {
            return m_network_point == e.m_network_point;
        }

        bool operator<(const server_met_entry& e) const
        {
            return m_network_point < e.m_network_point;
        }

        bool operator!=(const server_met_entry& e) const
        {
            return m_network_point != e.m_network_point;
        }
    };

    struct server_met
    {
        met_file_header                                                     m_header;
        container_holder<boost::uint32_t, std::deque<server_met_entry> >    m_servers;

        template<typename Archive>
        void serialize(Archive& ar)
        {
            ar & m_header;
            ar & m_servers;
        }
    };
}

#endif //__FILE__HPP__
