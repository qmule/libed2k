#ifndef __KAD_PACKET_STRUCT__
#define __KAD_PACKET_STRUCT__

#include "libed2k/packet_struct.hpp"

namespace libed2k {


// MOD Note: end
#define EDONKEYVERSION                  0x3C
#define KADEMLIA_VERSION1_46c           0x01 /*45b - 46c*/
#define KADEMLIA_VERSION2_47a           0x02 /*47a*/
#define KADEMLIA_VERSION3_47b           0x03 /*47b*/
#define KADEMLIA_VERSION5_48a           0x05 // -0.48a
#define KADEMLIA_VERSION6_49aBETA       0x06 // -0.49aBETA1, needs to support: OP_FWCHECKUDPREQ (!), obfuscation, direct callbacks, source type 6, UDP firewallcheck
#define KADEMLIA_VERSION7_49a           0x07 // -0.49a needs to support OP_KAD_FWTCPCHECK_ACK, KADEMLIA_FIREWALLED2_REQ
#define KADEMLIA_VERSION8_49b           0x08 // TAG_KADMISCOPTIONS, KADEMLIA2_HELLO_RES_ACK
#define KADEMLIA_VERSION9_50a           0x09 // handling AICH hashes on keyword storage
#define KADEMLIA_VERSION                KADEMLIA_VERSION5_48a // Change CT_EMULE_MISCOPTIONS2 if Kadversion becomes >= 15 (0x0F)


#define KADEMLIA_FIND_VALUE     0x02
#define KADEMLIA_STORE          0x04
#define KADEMLIA_FIND_NODE      0x0B
#define KADEMLIA_FIND_VALUE_MORE    KADEMLIA_FIND_NODE

#define KADEMLIA_TOLERANCE_ZONE 24

//#define KADEMLIA_VERSION    0x08    /* 0.49b */

    // COMMON KAD packages
    /**
      * KAD identifier compatible with eMule format serialization
      *
    */
    struct kad_id : public md4_hash {

        enum {
            kad_total_bits = md4_hash::size * 8
        };

        kad_id();
        kad_id(const md4_hash& h);
        kad_id(const md4_hash::md4hash_container&);

        operator md4_hash() {
            return md4_hash(m_hash);
        }

        /**
        * save/load as 4 32 bits digits in little endian save as 4 32 bits digits network byte order
        * rotate bytes in each 4 byte portion
        *
        */
        template<typename Archive>
        void save(Archive& ar) {
            for (unsigned i = 0; i < sizeof(m_hash); ++i) {
                ar & m_hash[(i / 4) * 4 + 3 - (i % 4)];
            }
        }

        template<typename Archive>
        void load(Archive& ar) {
            for (unsigned i = 0; i < sizeof(m_hash); ++i) {
                uint8_t c;
                ar & c;
                m_hash[(i / 4)*4 + 3 - (i % 4)] = c;
            }
        }

        LIBED2K_SERIALIZATION_SPLIT_MEMBER()
    };

    /**
      * more detailed analog of dht_tracker.state() from libtorrent
      * useful for gui and possibly for disk storage
      * 
    */
    struct kad_state_entry {
        net_identifier  point;
        kad_id          pid;
        uint16_t        timeout_count;

        kad_state_entry(client_id_type ip, uint16_t port, const kad_id& id, uint16_t tcount) {
            point.m_nIP = ip;
            point.m_nPort = port;
            pid = id;
            timeout_count = tcount;
        }
        
        template<typename Archive>
        void serialize(Archive& ar) {
            ar & point &  id & timeout_count;
        }
    };

    struct kad_state {
        kad_id  self_id;
        container_holder<uint16_t, std::deque<kad_state_entry> > entries;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & self_id & entries;
        }
    };

    struct kad_net_identifier {
        client_id_type  address;
        uint16_t        udp_port;
        uint16_t        tcp_port;

        template<typename Archive>
        void load(Archive& ar) {
            ar & address & udp_port & tcp_port;
            address = ntohl(address);
        }

        template<typename Archive>
        void save(Archive& ar) {
            client_id_type local_address = htonl(address);
            ar & local_address & udp_port & tcp_port;
        }

        LIBED2K_SERIALIZATION_SPLIT_MEMBER()
    };

    struct kad_entry {
        kad_id              kid;
        kad_net_identifier  address;
        uint8_t             version;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & kid & address & version;
        }
    };

    struct kad2_common_res {
        kad_id      kid;
        uint16_t    tcp_port;
        uint8_t     version;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & kid & tcp_port & version;
        }
    };

    struct kad_info_entry {
        kad_id hash;
        tag_list<uint8_t> tags;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & hash & tags;
        }
    };

    // BOOTSTRAP packages
    struct kad_bootstrap_req {
        kad_id  kid;
        kad_net_identifier  address;
        uint8_t tail;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & kid & address & tail;
        }
    };

    /**
     * this is common kad information reply in bootstrap and kademlia2_res
     * will use in observer reply call also
     */
    typedef std::deque<kad_entry>   kad_contacts_res;

    typedef container_holder<uint16_t, kad_contacts_res >   kad_bootstrap_res;

    struct kad2_bootstrap_req {

        template<typename Archive>
        void serialize(Archive& ar) {}
    };

    struct kad2_bootstrap_res {
        kad2_common_res client_info;
        container_holder<uint16_t, kad_contacts_res > contacts;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & client_info & contacts;
        }
    };

    // HELLO packages
    struct kad_hello_base {
        kad_id  kid;
        kad_net_identifier  address;
        uint8_t tail;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & kid & address & tail;
        }
    };

    struct kad_hello_req : public kad_hello_base {};
    struct kad_hello_res : public kad_hello_base {};

    struct kad2_hello_base {
        kad2_common_res     client_info;
        tag_list<uint8_t>   tags;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & client_info & tags;
        }
    };

    struct kad2_hello_req : public kad2_hello_base {};
    struct kad2_hello_res : public kad2_hello_base {};

    // SEARCH sources packages
    struct kademlia_req {
        uint8_t search_type;
        kad_id  kid_target;
        kad_id  kid_receiver;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & search_type & kid_target & kid_receiver;
        }
    };

    struct kademlia_res {
        kad_id  kid_target;
        container_holder<uint8_t, kad_contacts_res >    results;
        template<typename Archive>
        void serialize(Archive& ar) {
            ar & kid_target & results;
        }
    };

    struct kademlia2_req : public kademlia_req {};
    struct kademlia2_res : public kademlia_res {};

    struct kad_search_req {
        kad_id  target_id;

    };

    struct kad_search_res {
        kad_id  target_id;
        container_holder<uint16_t, std::deque<kad_info_entry> > results;

        template<typename Archive>
        void searialize(Archive& ar) {
            ar & target_id & results;
        }
    };


    /**
    * common KAD2 common search result for keywords, notes and sources 
    */
    struct kad2_search_res {
        kad_id  source_id;
        kad_id  target_id;
        container_holder<uint16_t, std::deque<kad_info_entry> > results;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & source_id & target_id & results;
        }
    };

    struct kad_search_notes_req {
        kad_id  target_id;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & target_id;
        }
    };

    struct kad_search_notes_res {
        kad_id  note_id;
        container_holder<uint16_t, std::deque<kad_info_entry> > notes;
    };

    struct kad2_search_sources_req {
        kad_id  target_id;
        uint16_t    start_position;
        uint64_t    size;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & target_id & start_position & size;
            start_position &= 0x7FFF;   // temp code
        }
    };

    struct kad2_search_key_req {
        kad_id  target_id;
        uint16_t start_position;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & target_id & start_position;
        }
    };

    struct kad2_search_notes_req {
        kad_id      target_id;
        uint64_t    filesize;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & target_id & filesize;
        }
    };

    // PUBLISH packages

    struct kad2_publish_key_req {
        kad_id  client_id;
        container_holder<uint16_t, std::deque<kad_info_entry> >  keys;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & client_id & keys;
        }
    };

    struct kad2_publish_source_req {
        kad_id  client_id;
        kad_id  source_id;
        tag_list<uint8_t>   tags;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & client_id & source_id & tags;
        }
    };


    struct kad_publish_note_req {
        kad_id  note_id;
        kad_id  publisher_id;
        tag_list<uint8_t>   tags;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & note_id & publisher_id & tags;
        }
    };

    struct kad2_publish_res {
        kad_id  target_id;
        uint8_t count;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & target_id; // &count;
            // read last field with catch exception - possibly isn't exist
        }
    };


    struct kad2_publish_key_res : public kad2_publish_res {};
    struct kad2_publish_source_res : public kad2_publish_res {};
    struct kad2_publish_note_res : public kad2_publish_res {};


    // FIREWALLED

    struct kad_firewalled_req {
        uint16_t    tcp_port;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & tcp_port;
        }
    };

    struct kad2_firewalled_req {
        uint16_t    tcp_port;
        kad_id      user_id;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & tcp_port & user_id;
        }
    };

    struct kad_firewalled_res {
        client_id_type ip;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & ip;
        }
    };

    struct kad_firewalled_ack_res {
        template<typename Archive>
        void serialize(Archive& ar) {}
    };

    struct kad2_firewalled_up {
        uint8_t error_code;
        uint16_t    incoming_port;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & error_code & incoming_port;
        }
    };


    // PING+PONG

    struct kad2_ping {
        template<typename Archive>
        void serialize(Archive& ar) {
        }
    };

    struct kad2_pong {
        uint16_t    udp_port;
        template<typename Archive>
        void serialize(Archive& ar) {
            ar & udp_port;
        }
    };

    struct kad_entry_ext {
        uint32_t    dw_key;
        uint32_t    dw_ip;
        uint8_t     verified;

        template<typename Archive>
        void serialize(Archive& ar) {
            ar & dw_key & dw_ip & verified;
        }
    };

    /**
      * this structure describes standard emule nodes.dat file internal structure
    */
    struct kad_nodes_dat {
        uint32_t    num_contacts;
        uint32_t    version;
        uint32_t    bootstrap_edition;
        container_holder<uint32_t, std::deque<kad_entry> >   bootstrap_container;
        std::list<kad_entry>        contacts_container;
        std::list<kad_entry_ext>    ext_container;

        template<typename Archive>
        void save(Archive& ar) {
            // back serialization hardcoded to version 1 to avoid useless emule features
            version = 1;
            num_contacts = 0;
            ar & num_contacts;
            ar & version;
            num_contacts = contacts_container.size();
            ar & num_contacts;
            for (std::list<kad_entry>::const_iterator itr = contacts_container.begin(); itr != contacts_container.end(); ++itr) {
                ar & *itr;
            }
        }

        template<typename Archive>
        void load(Archive& ar) {
            ar & num_contacts;
            if (num_contacts == 0) {
                ar & version;
                if (version == 3) {
                    ar & bootstrap_edition;
                    if (bootstrap_edition == 1) {
                        ar & bootstrap_container;
                        return;
                    }
                }

                if (version >= 1 && version <= 3) {
                    ar & num_contacts;
                }
            }

            for (size_t i = 0; i != num_contacts; ++i) {
                kad_entry ke;
                kad_entry_ext kee;
                ar & ke;
                contacts_container.push_back(ke);
                if (version >= 2) {
                    ar & kee;
                    ext_container.push_back(kee);
                }
            }
        }

        LIBED2K_SERIALIZATION_SPLIT_MEMBER()
    };

    template<> struct packet_type<sources_request>{
        static const proto_type value       = OP_REQUESTSOURCES;
        static const proto_type protocol    = OP_EMULEPROT;
    };
    template<> struct packet_type<sources_request2>{
        static const proto_type value       = OP_REQUESTSOURCES2;
        static const proto_type protocol    = OP_EMULEPROT;
    };
    template<> struct packet_type<sources_answer>{
        static const proto_type value       = OP_ANSWERSOURCES;
        static const proto_type protocol    = OP_EMULEPROT;
    };
    template<> struct packet_type<sources_answer2>{
        static const proto_type value       = OP_ANSWERSOURCES2;
        static const proto_type protocol    = OP_EMULEPROT;
    };

    template<> struct packet_type<kad2_ping> {
        static const proto_type value = KADEMLIA2_PING;
        static const proto_type protocol = OP_KADEMLIAHEADER;
    };

    template<> struct packet_type<kad2_pong> {
        static const proto_type value = KADEMLIA2_PONG;
        static const proto_type protocol = OP_KADEMLIAHEADER;
    };

    template<> struct packet_type<kad2_hello_req> {
        static const proto_type value = KADEMLIA2_HELLO_REQ;
        static const proto_type protocol = OP_KADEMLIAHEADER;
    };

    template<> struct packet_type<kad2_hello_res> {
        static const proto_type value = KADEMLIA2_HELLO_RES;
        static const proto_type protocol = OP_KADEMLIAHEADER;
    };

    template<> struct packet_type<kad2_bootstrap_req> {
        static const proto_type value = KADEMLIA2_BOOTSTRAP_REQ;
        static const proto_type protocol = OP_KADEMLIAHEADER;
    };

    template<> struct packet_type<kad2_bootstrap_res> {
        static const proto_type value = KADEMLIA2_BOOTSTRAP_RES;
        static const proto_type protocol = OP_KADEMLIAHEADER;
    };

    template<> struct packet_type<kad_firewalled_req> {
        static const proto_type value = KADEMLIA_FIREWALLED_REQ;
        static const proto_type protocol = OP_KADEMLIAHEADER;
    };

    template<> struct packet_type<kad_firewalled_res> {
        static const proto_type value = KADEMLIA_FIREWALLED_RES;
        static const proto_type protocol = OP_KADEMLIAHEADER;
    };

    template<> struct packet_type<kademlia2_req> {
        static const proto_type value = KADEMLIA2_REQ;
        static const proto_type protocol = OP_KADEMLIAHEADER;
    };

    template<> struct packet_type<kademlia2_res> {
        static const proto_type value = KADEMLIA2_RES;
        static const proto_type protocol = OP_KADEMLIAHEADER;
    };

    template<> struct packet_type<kad2_search_key_req> {
        static const proto_type value = KADEMLIA2_SEARCH_KEY_REQ;
        static const proto_type protocol = OP_KADEMLIAHEADER;
    };

    template<> struct packet_type<kad2_search_notes_req> {
      static const proto_type value = KADEMLIA2_SEARCH_NOTES_REQ;
      static const proto_type protocol = OP_KADEMLIAHEADER;
    };

    template<> struct packet_type<kad2_search_sources_req> {
      static const proto_type value = KADEMLIA2_SEARCH_SOURCE_REQ;
      static const proto_type protocol = OP_KADEMLIAHEADER;
    };

    template<> struct packet_type<kad2_search_res> {
        static const proto_type value = KADEMLIA2_SEARCH_RES;
        static const proto_type protocol = OP_KADEMLIAHEADER;
    };

    /**
    *   special transaction identifier on packet type
    */
    template<typename T> struct transaction_identifier;

    template<> struct transaction_identifier<kad2_ping> { static const uint16_t id = 'p'; };
    template<> struct transaction_identifier<kad2_pong> { static const uint16_t id = 'p'; };

    // bootstrap request/response identifiers
    template<> struct transaction_identifier<kad2_bootstrap_req> {  static const uint16_t id = 'b'; };
    template<> struct transaction_identifier<kad2_bootstrap_res> {  static const uint16_t id = 'b'; };

    // hello request/response
    template<> struct transaction_identifier<kad2_hello_req> { static const uint16_t id = 'h'; };
    template<> struct transaction_identifier<kad2_hello_res> { static const uint16_t id = 'h'; };

    // kademlia request/response
    template<> struct transaction_identifier<kademlia2_req> { static const uint16_t id = 'l'; };
    template<> struct transaction_identifier<kademlia2_res> { static const uint16_t id = 'l'; };

    // kademlia search keywords, notes and sources
    // equal identifiers since response is one type for all these requests
    template<> struct transaction_identifier<kad2_search_key_req> { static const uint16_t id = 's'; };
    template<> struct transaction_identifier<kad2_search_notes_req> { static const uint16_t id = 's'; };
    template<> struct transaction_identifier<kad2_search_sources_req> { static const uint16_t id = 's'; };

    // we have one packet format for all three requests above
    template<> struct transaction_identifier<kad2_search_res> { static const uint16_t id = 's';  };


    // firewalled 
    template<> struct transaction_identifier<kad_firewalled_req> { static const uint16_t id = 'f';  };
    template<> struct transaction_identifier<kad_firewalled_res> { static const uint16_t id = 'f';  };
}

#endif //__KAD_PACKET_STRUCT__
