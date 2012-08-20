#ifndef __LIBED2K_SESSION_SETTINGS__
#define __LIBED2K_SESSION_SETTINGS__

#include <limits>
#include <libed2k/md4_hash.hpp>
#include <libed2k/constants.hpp>

namespace libed2k
{
    class session_settings
    {
    public:
        typedef std::vector<std::pair<std::string, bool> >  fd_list;

        session_settings():
            server_timeout(220)
            , peer_timeout(120)
            , peer_connect_timeout(7)
            , block_request_timeout(BLOCK_SIZE / 1024)
            , allow_multiple_connections_per_ip(false)
            , recv_socket_buffer_size(0)
            , send_socket_buffer_size(0)
            , send_buffer_watermark(100 * 1024)
            , server_port(4661)
            , listen_port(4662)
            , client_name("libed2k")
            , server_keep_alive_timeout(200)
            , server_reconnect_timeout(5)
            , max_peerlist_size(4000)
            , download_rate_limit(-1)
            , upload_rate_limit(-1)
            , unchoke_slots_limit(8)
            , m_version(0x3c)
            , m_max_announces_per_call(100)
            , m_announce_timeout(-1)
            , m_show_shared_catalogs(true)
            , m_show_shared_files(true)
            , user_agent(md4_hash::emule)
            , ignore_resume_timestamps(false)
            , no_recheck_incomplete_resume(false)
            // Disk IO settings
            , file_pool_size(40)
            , max_queued_disk_bytes(16*1024*1024)
            , max_queued_disk_bytes_low_watermark(0)
            , cache_size((16*1024*1024) / BLOCK_SIZE)
            , cache_buffer_chunk_size((16*16*1024) / BLOCK_SIZE)
            , cache_expiry(5*60)
            , use_read_cache(true)
            , explicit_read_cache(false)
            , disk_io_write_mode(0)
            , disk_io_read_mode(0)
            , coalesce_reads(false)
            , coalesce_writes(false)
            , optimize_hashing_for_speed(true)
            , file_checks_delay_per_block(0)
            , disk_cache_algorithm(avoid_readback)
            , read_cache_line_size((32*16*1024) / BLOCK_SIZE)
            , write_cache_line_size((32*16*1024) / BLOCK_SIZE)
            , disable_hash_checks(false)
            , allow_reordered_disk_operations(true)
#ifndef LIBED2K_DISABLE_MLOCK
            , lock_disk_cache(false)
#endif
            , volatile_read_cache(false)
            , default_cache_min_age(1)
            , no_atime_storage(true)
            , read_job_every(10)
            , use_disk_read_ahead(true)
            , lock_files(false)
            , low_prio_disk(true)
        {
        }

        // the number of seconds to wait for any activity on
        // the server wire before closing the connection due
        // to time out.
        int server_timeout;

        // the number of seconds to wait for any activity on
        // the peer wire before closing the connection due
        // to time out.
        int peer_timeout;

        // this is the timeout for a connection attempt. If
        // the connect does not succeed within this time, the
        // connection is dropped. The time is specified in seconds.
        int peer_connect_timeout;

        // the number of seconds to wait for block request.
        int block_request_timeout;

        // false to not allow multiple connections from the same
        // IP address. true will allow it.
        bool allow_multiple_connections_per_ip;

        // sets the socket send and receive buffer sizes
        // 0 means OS default
        int recv_socket_buffer_size;
        int send_socket_buffer_size;

        // if the send buffer has fewer bytes than this, we'll
        // read another 16kB block onto it. If set too small,
        // upload rate capacity will suffer. If set too high,
        // memory will be wasted.
        // The actual watermark may be lower than this in case
        // the upload rate is low, this is the upper limit.
        int send_buffer_watermark;

        // ed2k server hostname
        std::string server_hostname;
        // ed2k server port
        int server_port;
        // ed2k peer port for incoming peer connections
        int listen_port;
        // ed2k client name
        std::string client_name;
        int server_keep_alive_timeout;
        // reconnect to server after fail, -1 - do nothing
        int server_reconnect_timeout;

        // the max number of peers in the peer list
        // per transfer. This is the peers we know
        // about, not necessarily connected to.
        int max_peerlist_size;

        /**
          * session rate limits
          * -1 unlimits
         */
        int download_rate_limit;
        int upload_rate_limit;

        // the max number of unchoke slots in the session (might be
        // overridden by unchoke algorithm)
        int unchoke_slots_limit;

        unsigned short m_version;
        unsigned short m_max_announces_per_call;

        /**
          * announce timeout in seconds
          * -1 - announces off
         */
        int m_announce_timeout;
        bool m_show_shared_catalogs;    //!< show shared catalogs to client
        bool m_show_shared_files;       //!< show shared files to client
        md4_hash user_agent;            //!< ed2k client hash - user agent information

        //!< known.met file
        std::string m_known_file;

        //!< users files and directories
        //!< second parameter true for recursive search and false otherwise
        fd_list m_fd_list;

        /**
          * root directory for auto-creating collections
          * collection will create when we share some folder
         */
        std::string m_collections_directory;

        // when set to true, the file modification time is ignored when loading
        // resume data. The resume data includes the expected timestamp of each
        // file and is typically compared to make sure the files haven't changed
        // since the last session
        bool ignore_resume_timestamps;

        // normally, if a resume file is incomplete (typically there's no
        // "file sizes" field) the transfer is queued for a full check. If
        // this settings is set to true, instead libed2k will assume
        // we have none of the files and go straight to download
        bool no_recheck_incomplete_resume;

        /********************
         * Disk IO settings *
         ********************/

        // sets the upper limit on the total number of files this
        // session will keep open. The reason why files are
        // left open at all is that some anti virus software
        // hooks on every file close, and scans the file for
        // viruses. deferring the closing of the files will
        // be the difference between a usable system and
        // a completely hogged down system. Most operating
        // systems also has a limit on the total number of
        // file descriptors a process may have open. It is
        // usually a good idea to find this limit and set the
        // number of connections and the number of files
        // limits so their sum is slightly below it.
        int file_pool_size;

        // the maximum number of bytes a connection may have
        // pending in the disk write queue before its download
        // rate is being throttled. This prevents fast downloads
        // to slow medias to allocate more and more memory
        // indefinitely. This should be set to at least 16 kB
        // to not completely disrupt normal downloads. If it's
        // set to 0, you will be starving the disk thread and
        // nothing will be written to disk.
        // this is a per session setting.
        int max_queued_disk_bytes;

        // this is the low watermark for the disk buffer queue.
        // whenever the number of queued bytes exceed the
        // max_queued_disk_bytes, libed2k will wait for
        // it to drop below this value before issuing more
        // reads from the sockets. If set to 0, the
        // low watermark will be half of the max queued disk bytes
        int max_queued_disk_bytes_low_watermark;

        // the disk write cache, specified in BLOCK_SIZE blocks.
        // default is 16 MiB / BLOCK_SIZE. -1 means automatic, which
        // adjusts the cache size depending on the amount
        // of physical RAM in the machine.
        int cache_size;

        // this is the number of disk buffer blocks (BLOCK_SIZE)
        // that should be allocated at a time. It must be
        // at least 1. Lower number saves memory at the expense
        // of more heap allocations
        int cache_buffer_chunk_size;

        // the number of seconds a write cache entry sits
        // idle in the cache before it's forcefully flushed
        // to disk. Default is 5 minutes.
        int cache_expiry;

        // when true, the disk I/O thread uses the disk
        // cache for caching blocks read from disk too
        bool use_read_cache;

        // don't implicitly cache pieces in the read cache,
        // only cache pieces that are explicitly asked to be
        // cached.
        bool explicit_read_cache;

        enum io_buffer_mode_t
        {
            enable_os_cache = 0,
            disable_os_cache_for_aligned_files = 1,
            disable_os_cache = 2
        };
        int disk_io_write_mode;
        int disk_io_read_mode;

        bool coalesce_reads;
        bool coalesce_writes;

        // if this is set to false, the hashing will be
        // optimized for memory usage instead of the
        // number of read operations
        bool optimize_hashing_for_speed;

        // if > 0, file checks will have a short
        // delay between disk operations, to make it 
        // less intrusive on the system as a whole
        // blocking the disk. This delay is specified
        // in milliseconds and the delay will be this
        // long per BLOCK_SIZE block
        // the default of 10 ms/16kiB will limit
        // the checking rate to 1.6 MiB per second
        int file_checks_delay_per_block;

        enum disk_cache_algo_t
        { lru, largest_contiguous, avoid_readback };

        disk_cache_algo_t disk_cache_algorithm;

        // the number of blocks that will be read ahead
        // when reading a block into the read cache
        int read_cache_line_size;

        // whenever a contiguous range of this many
        // blocks is found in the write cache, it
        // is flushed immediately
        int write_cache_line_size;

        // when set to true, all data downloaded from
        // peers will be assumed to be correct, and not
        // tested to match the hashes in the torrent
        // this is only useful for simulation and
        // testing purposes (typically combined with
        // disabled_storage)
        bool disable_hash_checks;

        // if this is true, disk read operations may
        // be re-ordered based on their physical disk
        // read offset. This greatly improves throughput
        // when uploading to many peers. This assumes
        // a traditional hard drive with a read head
        // and spinning platters. If your storage medium
        // is a solid state drive, this optimization
        // doesn't give you an benefits
        bool allow_reordered_disk_operations;

#ifndef TORRENT_DISABLE_MLOCK
        // if this is set to true, the memory allocated for the
        // disk cache will be locked in physical RAM, never to
        // be swapped out
        bool lock_disk_cache;
#endif

        // if this is set to true, any block read from the
        // disk cache will be dropped from the cache immediately
        // following. This may be useful if the block is not
        // expected to be hit again. It would save some memory
        bool volatile_read_cache;

        // this is the default minimum time any read cache line
        // is kept in the cache.
        int default_cache_min_age;

        // if set to true, files won't have their atime updated
        // on disk reads. This works on linux
        bool no_atime_storage;

        // to avoid write jobs starving read jobs, if this many
        // write jobs have been taking priority in a row, service
        // one read job
        int read_job_every;

        // issue posix_fadvise() or fcntl(F_RDADVISE) for disk reads
        // ahead of time
        bool use_disk_read_ahead;

        // if set to true, files will be locked when opened.
        // preventing any other process from modifying them
        bool lock_files;

        // if this is set to true, the disk I/O will be
		// run at lower-than-normal priority. This is
		// intended to make the machine more responsive
		// to foreground tasks, while bittorrent runs
		// in the background
        bool low_prio_disk;

    };

}

#endif
