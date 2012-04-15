#ifndef __PACKET_STRUCT__
#define __PACKET_STRUCT__

#include <vector>
#include <boost/cstdint.hpp>
#include "ctag.hpp"

namespace libed2k
{
    /**
      *  common container holder structure
     */
    template<typename size_type, class collection_type>
    struct container_holder
    {
        size_type       m_size;
        collection_type m_collection;
        typedef typename collection_type::iterator Iterator;
        typedef typename collection_type::value_type elem;

        template<typename Archive>
        void save(Archive& ar)
        {
            // save collection size in field with proprly size
            m_size = static_cast<size_type>(m_collection.size());
            ar & m_size;

            for(Iterator i = m_collection.begin(); i != m_collection.end(); ++i)
            {
                ar & *i;
            }

        }

        template<typename Archive>
        void load(Archive& ar)
        {
            ar & m_size;
            for (size_type i = 0; i < m_size; i++)
            {
                elem e;
                ar & e;
                *(std::back_inserter(m_collection)) = e;
                //std::back_insert_iterator< collection_type > back_it (m_collection);
            }
        }

        LIBED2K_SERIALIZATION_SPLIT_MEMBER()
    };

    struct libed2k_header
    {
        typedef boost::uint8_t  proto_type;
        typedef boost::uint32_t size_type;

        proto_type  m_protocol;
        size_type   m_size;
        proto_type  m_type;
    };

    // common protocol structures

    // file id/port in shared file entry with LowID client
    const boost::uint32_t FILE_COMPLETE_ID      = 0xfbfbfbfbU;
    const boost::uint16_t FILE_COMPLETE_PORT    = 0xfbfbU;
    const boost::uint32_t FILE_INCOMPLETE_ID    = 0xfcfcfcfcU;
    const boost::uint16_t FILE_INCOMPLETE_PORT  = 0xfcfcU;

    struct server_address
    {
        boost::uint32_t m_nIP;
        boost::uint16_t m_nPort;

        template<typename Archive>
        void serialize(Archive& ar)
        {
            ar & m_nIP;
            ar & m_nPort;
        }
    };

    /**
      * shared file structure
     */
    struct shared_file_entry
    {
        md4_hash                    m_hFile;    //!< md4 file hash
        boost::uint32_t             m_nFileId;  //!< client id for HighID
        boost::uint16_t             m_nPort;    //!< client port for HighID
        tag_list<boost::uint16_t>   m_list;     //!< file information list


        shared_file_entry();
        shared_file_entry(const md4_hash& hFile, boost::uint32_t nFileId, boost::uint16_t nPort);

        template<typename Archive>
        void serialize(Archive& ar)
        {
            ar & m_hFile;
            ar & m_nFileId;
            ar & m_nPort;
            ar & m_list;
        }
    };

    //!< client <-> server messages
    //!< CS = client to server
    //!< SC = server to client


    /**
      * login request structure - contain some info and 4 tag items
     */
    struct cs_login_request
    {
        md4_hash                    m_hClient;
        boost::uint32_t             m_nClientId;
        boost::uint16_t             m_nPort;
        tag_list<boost::uint16_t>   m_list;
    };

    /**
      * empty structure for get servers list from server
     */
    typedef container_holder<boost::uint8_t, std::vector<server_address> > server_list;
    typedef container_holder<boost::uint32_t, std::vector<shared_file_entry> > shared_files_list;

    struct server_info
    {
        md4_hash                    m_hServer;
        server_address              m_address;
        tag_list<boost::uint32_t>   m_list;
    };

}


#endif //__PACKET_STRUCT__
