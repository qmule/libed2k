#ifndef __LIBED2K_TRANSFER_HANDLE__
#define __LIBED2K_TRANSFER_HANDLE__

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <libed2k/peer_info.hpp>
#include "libed2k/md4_hash.hpp"
#include "libed2k/config.hpp"
#include "libed2k/storage_defs.hpp"
#include "libed2k/address.hpp"
#include "libed2k/filesystem.hpp"

namespace libed2k
{
    class transfer;
    namespace aux
    {
        class session_impl_base;
        class session_impl;
        class session_impl_test;
        class session_fast_rd;
    }

    struct LIBED2K_EXPORT transfer_status
    {
        transfer_status()
            : state(checking_resume_data)
            , paused(false)
            , progress(0.f)
            , progress_ppm(0)
            , total_download(0)
            , total_upload(0)
            , total_payload_download(0)
            , total_payload_upload(0)
            , total_failed_bytes(0)
            , total_redundant_bytes(0)
            , download_rate(0)
            , upload_rate(0)
            , download_payload_rate(0)
            , upload_payload_rate(0)
            , num_seeds(0)
            , num_peers(0)
            , num_complete(-1)
            , num_incomplete(-1)
            , list_seeds(0)
            , list_peers(0)
            , connect_candidates(0)
            , num_pieces(0)
            , total_done(0)
            , total_wanted_done(0)
            , total_wanted(0)
            , distributed_full_copies(0)
            , distributed_fraction(0)
            , distributed_copies(0.f)
            , block_size(0)
            , num_uploads(0)
            , num_connections(0)
            , uploads_limit(0)
            , connections_limit(0)
            , storage_mode(storage_mode_sparse)
            , up_bandwidth_queue(0)
            , down_bandwidth_queue(0)
            , all_time_upload(0)
            , all_time_download(0)
            , active_time(0)
            , finished_time(0)
            , seeding_time(0)
            , seed_rank(0)
            , last_scrape(0)
            , has_incoming(false)
            , sparse_regions(0)
            , seed_mode(false)
            , upload_mode(false)
            , priority(0)
        {}

        enum state_t
        {
            queued_for_checking,
            checking_files,
            downloading_metadata,
            downloading,
            finished,
            seeding,
            allocating,
            checking_resume_data
        };

        state_t state;
        bool paused;
        float progress;
        // progress parts per million (progress * 1000000)
        // when disabling floating point operations, this is
        // the only option to query progress
        int progress_ppm;
        std::string error;

        boost::posix_time::time_duration next_announce;
        boost::posix_time::time_duration announce_interval;

        std::string current_tracker;

        // transferred this session!
        // total, payload plus protocol
        size_type total_download;
        size_type total_upload;

        // payload only
        size_type total_payload_download;
        size_type total_payload_upload;

        // the amount of payload bytes that
        // has failed their hash test
        size_type total_failed_bytes;

        // the number of payload bytes that
        // has been received redundantly.
        size_type total_redundant_bytes;

        // current transfer rate
        // payload plus protocol
        int download_rate;
        int upload_rate;

        // the rate of payload that is
        // sent and received
        int download_payload_rate;
        int upload_payload_rate;

        // the number of peers this torrent is connected to
        // that are seeding.
        int num_seeds;

        // the number of peers this torrent
        // is connected to (including seeds).
        int num_peers;

        // if the tracker sends scrape info in its
        // announce reply, these fields will be
        // set to the total number of peers that
        // have the whole file and the total number
        // of peers that are still downloading
        int num_complete;
        int num_incomplete;

        // this is the number of seeds whose IP we know
        // but are not necessarily connected to
        int list_seeds;

        // this is the number of peers whose IP we know
        // (including seeds), but are not necessarily
        // connected to
        int list_peers;

        // the number of peers in our peerlist that
        // we potentially could connect to
        int connect_candidates;

        bitfield pieces;

        // this is the number of pieces the client has
        // downloaded. it is equal to:
        // std::accumulate(pieces->begin(), pieces->end());
        int num_pieces;

        // the number of bytes of the file we have
        // including pieces that may have been filtered
        // after we downloaded them
        size_type total_done;

        // the number of bytes we have of those that we
        // want. i.e. not counting bytes from pieces that
        // are filtered as not wanted.
        size_type total_wanted_done;

        // the total number of bytes we want to download
        // this may be smaller than the total torrent size
        // in case any pieces are filtered as not wanted
        size_type total_wanted;

        // the number of distributed copies of the file.
        // note that one copy may be spread out among many peers.
        //
        // the integer part tells how many copies
        //   there are of the rarest piece(s)
        //
        // the fractional part tells the fraction of pieces that
        //   have more copies than the rarest piece(s).

        // the number of full distributed copies (i.e. the number
        // of peers that have the rarest piece)
        int distributed_full_copies;

        // the fraction of pieces that more peers has than the
        // rarest pieces. This indicates how close the swarm is
        // to have one more full distributed copy
        int distributed_fraction;

        float distributed_copies;

        // the block size that is used in this torrent. i.e.
        // the number of bytes each piece request asks for
        // and each bit in the download queue bitfield represents
        int block_size;

        int num_uploads;
        int num_connections;
        int uploads_limit;
        int connections_limit;

        // true if the torrent is saved in compact mode
        // false if it is saved in full allocation mode
        storage_mode_t storage_mode;

        int up_bandwidth_queue;
        int down_bandwidth_queue;

        // number of bytes downloaded since torrent was started
        // saved and restored from resume data
        size_type all_time_upload;
        size_type all_time_download;

        // the number of seconds of being active
        // and as being a seed, saved and restored
        // from resume data
        int active_time;
        int finished_time;
        int seeding_time;

        // higher value means more important to seed
        int seed_rank;

        // number of seconds since last scrape, or -1 if
        // there hasn't been a scrape
        int last_scrape;

        // true if there are incoming connections to this
        // torrent
        bool has_incoming;

        // the number of "holes" in the torrent
        int sparse_regions;

        // is true if this torrent is (still) in seed_mode
        bool seed_mode;

        // this is set to true when the torrent is blocked
        // from downloading, typically caused by a file
        // write operation failing
        bool upload_mode;

        // the priority of this torrent
        int priority;
    };

    // We will usually have to store our transfer handles somewhere, 
    // since it's the object through which we retrieve information 
    // about the transfer and aborts the transfer.
    struct transfer_handle
    {
        friend class aux::session_impl_base;
        friend class aux::session_impl;
        friend class aux::session_impl_test;
        friend class aux::session_fast_rd;
        friend class transfer;

        transfer_handle() {}

        bool is_valid() const;

        md4_hash hash() const;
        std::string filepath() const;
        size_type filesize() const;
        bool is_seed() const;
        bool is_finished() const;
        bool is_paused() const;
        bool is_aborted() const;
        bool is_announced() const;
        transfer_status status() const;
        void get_peer_info(std::vector<peer_info>& infos) const;

        void piece_availability(std::vector<int>& avail) const;
        void set_piece_priority(int index, int priority) const;
        int piece_priority(int index) const;
        bool is_sequential_download() const;
        void set_sequential_download(bool sd) const;
        void set_upload_limit(int limit) const;
        int upload_limit() const;
        void set_download_limit(int limit) const;
        int download_limit() const;

        void pause() const;
        void resume() const;

        size_t num_pieces() const;
        int num_peers() const;
        int num_seeds() const;
        std::string save_path() const;
        void save_resume_data() const;

        void move_storage(std::string const& save_path) const;
        bool rename_file(const std::string& name) const;

        bool operator==(const transfer_handle& h) const
        { return m_transfer.lock() == h.m_transfer.lock(); }

        bool operator!=(const transfer_handle& h) const
        { return m_transfer.lock() != h.m_transfer.lock(); }

        bool operator<(const transfer_handle& h) const
        { return m_transfer.lock() < h.m_transfer.lock(); }
    private:

        transfer_handle(const boost::weak_ptr<transfer>& t):
            m_transfer(t)
        {}

        boost::weak_ptr<transfer> m_transfer;
    };

    struct LIBED2K_EXPORT block_info
    {
        enum block_state_t
        { none, requested, writing, finished };

    private:
        LIBED2K_UNION addr_t
        {
            address_v4::bytes_type v4;
#if LIBED2K_USE_IPV6
            address_v6::bytes_type v6;
#endif
        } addr;

        boost::uint16_t port;
    public:

        void set_peer(tcp::endpoint const& ep)
        {
#if LIBED2K_USE_IPV6
            is_v6_addr = ep.address().is_v6();
            if (is_v6_addr)
                addr.v6 = ep.address().to_v6().to_bytes();
            else
#endif
                addr.v4 = ep.address().to_v4().to_bytes();
            port = ep.port();
        }

        tcp::endpoint peer() const
        {
#if LIBED2K_USE_IPV6
            if (is_v6_addr)
                return tcp::endpoint(address_v6(addr.v6), port);
            else
#endif
                return tcp::endpoint(address_v4(addr.v4), port);
        }

        // number of bytes downloaded in this block
        unsigned bytes_progress:15;
        // the total number of bytes in this block
        unsigned block_size:15;
    private:
        // the type of the addr union
        unsigned is_v6_addr:1;
        unsigned unused:1;
    public:
        // the state this block is in (see block_state_t)
        unsigned state:2;
        // the number of peers that has requested this block
        // typically 0 or 1. If > 1, this block is in
        // end game mode
        unsigned num_peers:14;
    };


    extern std::string transfer_status2string(const transfer_status& s);
}

#endif
