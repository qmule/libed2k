#ifndef __LIBED2K_ALERT_TYPES__
#define __LIBED2K_ALERT_TYPES__

// for print_endpoint
#include <libtorrent/socket.hpp>


#include "libed2k/alert.hpp"
#include "libed2k/types.hpp"
#include "libed2k/error_code.hpp"
#include "libed2k/packet_struct.hpp"
#include "libed2k/transfer_handle.hpp"


namespace libed2k
{
    struct server_name_resolved_alert : alert
    {
        const static int static_category = alert::status_notification;
        server_name_resolved_alert(const std::string& strServer) : m_strServer(strServer){}
        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new server_name_resolved_alert(*this));
        }

        virtual std::string message() const { return std::string("server name was resolved"); }
        virtual char const* what() const { return "server notification"; }

        std::string m_strServer;
    };

    /**
      * emit when server connection was initialized
     */
    struct server_connection_initialized_alert : alert
    {
        const static int static_category = alert::status_notification;

        server_connection_initialized_alert(boost::uint32_t nClientId,
                boost::uint32_t nTCPFlags, boost::uint32_t nAuxPort) :
                    m_nClientId(nClientId),
                    m_nTCPFlags(nTCPFlags),
                    m_nAuxPort(nAuxPort)
        {}

        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new server_connection_initialized_alert(*this));
        }

        virtual std::string message() const { return std::string("server connection was initialized"); }
        virtual char const* what() const { return "server notification"; }

        boost::uint32_t m_nClientId;
        boost::uint32_t m_nTCPFlags;
        boost::uint32_t m_nAuxPort;
    };

    /**
      * emit on OP_SERVERSTATUS
     */
    struct server_status_alert : alert
    {
        const static int static_category = alert::status_notification | alert::server_notification;

        server_status_alert(boost::uint32_t nFilesCount, boost::uint32_t nUsersCount) :
            m_nFilesCount(nFilesCount), m_nUsersCount(nUsersCount)
        {
        }

        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new server_status_alert(*this));
        }

        virtual std::string message() const { return std::string("server status information"); }
        virtual char const* what() const { return "server status information"; }

        boost::uint32_t m_nFilesCount;
        boost::uint32_t m_nUsersCount;
    };

    /**
      * emit on OP_SERVERIDENT
     */

    struct server_identity_alert : alert
    {
        const static int static_category = alert::status_notification | alert::server_notification;

        server_identity_alert(const md4_hash& hServer , net_identifier address, const std::string& strName, const std::string& strDescr) :
            m_hServer(hServer), m_address(address), m_strName(strName), m_strDescr(strDescr)
        {
        }

        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new server_identity_alert(*this));
        }

        virtual std::string message() const { return std::string("server identity information"); }
        virtual char const* what() const { return "server identity information"; }

        md4_hash        m_hServer;
        net_identifier  m_address;
        std::string     m_strName;
        std::string     m_strDescr;
    };

    /**
      * emit for every server message
     */
    struct server_message_alert: alert
    {
        const static int static_category = alert::server_notification;

        server_message_alert(const std::string& strMessage) : m_strMessage(strMessage){}
        virtual int category() const { return static_category; }

        virtual std::string message() const { return m_strMessage; }
        virtual char const* what() const { return "server message incoming"; }

        virtual std::auto_ptr<alert> clone() const
        {
            return (std::auto_ptr<alert>(new server_message_alert(*this)));
        }

        std::string m_strMessage;
    };

    /**
      * emit when server connection closed
     */
    struct server_connection_closed : alert
    {
        const static int static_category = alert::status_notification | alert::server_notification;

        server_connection_closed(const error_code& error) : m_error(error){}
        virtual int category() const { return static_category; }

        virtual std::string message() const { return m_error.message(); }
        virtual char const* what() const { return "server connection closed"; }

        virtual std::auto_ptr<alert> clone() const
        {
            return (std::auto_ptr<alert>(new server_connection_closed(*this)));
        }

        error_code m_error;
    };

    struct mule_listen_failed_alert: alert
    {
        mule_listen_failed_alert(tcp::endpoint const& ep, error_code const& ec):
            endpoint(ep), error(ec)
        {}

        tcp::endpoint endpoint;
        error_code error;

        virtual std::auto_ptr<alert> clone() const
        { return std::auto_ptr<alert>(new mule_listen_failed_alert(*this)); }
        virtual char const* what() const { return "listen failed"; }
        const static int static_category = alert::status_notification | alert::error_notification;
        virtual int category() const { return static_category; }
        virtual std::string message() const
        {
            char ret[200];
            snprintf(ret, sizeof(ret), "mule listening on %s failed: %s"
                , libtorrent::print_endpoint(endpoint).c_str(), error.message().c_str());
            return ret;
        }

    };

    struct peer_alert : alert
    {
        const static int static_category = alert::peer_notification;
        peer_alert(const net_identifier& np, const md4_hash& hash) : m_np(np), m_hash(hash)
        {}

        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new peer_alert(*this));
        }

        virtual std::string message() const { return std::string("peer alert"); }
        virtual char const* what() const { return "peer alert"; }

        net_identifier  m_np;
        md4_hash        m_hash;
    };

    /**
      * this alert throws on server search results and on user shared files
     */
    struct shared_files_alert : peer_alert
    {
        const static int static_category = alert::server_notification | alert::peer_notification;

        shared_files_alert(const net_identifier& np, const md4_hash& hash, const shared_files_list& files, bool more) :
            peer_alert(np, hash),
            m_files(files),
            m_more(more){}
        virtual int category() const { return static_category; }

        virtual std::string message() const { return "search result from string"; }
        virtual char const* what() const { return "search result from server"; }

        virtual std::auto_ptr<alert> clone() const
        {
            return (std::auto_ptr<alert>(new shared_files_alert(*this)));
        }

        shared_files_list       m_files;
        bool                    m_more;
    };

    struct shared_directories_alert : peer_alert
    {
        const static int static_category = alert::peer_notification;

        shared_directories_alert(const net_identifier& np, const md4_hash& hash, const client_shared_directories_answer& dirs) :
            peer_alert(np, hash)
        {
            for (size_t n = 0; n < dirs.m_dirs.m_collection.size(); ++n)
            {
                m_dirs.push_back(dirs.m_dirs.m_collection[n].m_collection);
            }
        }

        virtual int category() const { return static_category; }

        virtual std::string message() const { return "search result from string"; }
        virtual char const* what() const { return "search result from server"; }

        virtual std::auto_ptr<alert> clone() const
        {
            return (std::auto_ptr<alert>(new shared_directories_alert(*this)));
        }

        std::vector<std::string>    m_dirs;
    };

    /**
      * this alert throws on server search results and on user shared files
     */
    struct shared_directory_files_alert : shared_files_alert
    {
        const static int static_category = alert::peer_notification;

        shared_directory_files_alert(const net_identifier& np,
                const md4_hash& hash,
                const std::string& strDirectory,
                const shared_files_list& files) :
            shared_files_alert(np, hash, files, false), m_strDirectory(strDirectory)
        {
        }

        virtual int category() const { return static_category; }

        virtual std::string message() const { return "search result for directory from peer"; }
        virtual char const* what() const { return "search result for directory from peer"; }

        virtual std::auto_ptr<alert> clone() const
        {
            return (std::auto_ptr<alert>(new shared_directory_files_alert(*this)));
        }

        std::string m_strDirectory;
    };

    struct ismod_shared_directory_files_alert : shared_files_alert
    {
        const static int static_category = alert::peer_notification;

        ismod_shared_directory_files_alert(const net_identifier& np,
                const md4_hash& hash,
                const md4_hash& dir_hash,
                const shared_files_list& files) :
            shared_files_alert(np, hash, files, false), m_dir_hash(dir_hash)
        {
        }

        virtual int category() const { return static_category; }

        virtual std::string message() const { return "search result for directory from peer"; }
        virtual char const* what() const { return "search result for directory from peer"; }

        virtual std::auto_ptr<alert> clone() const
        {
            return (std::auto_ptr<alert>(new ismod_shared_directory_files_alert(*this)));
        }

        md4_hash m_dir_hash;
    };

    struct peer_connected_alert : peer_alert
    {

        virtual int category() const { return static_category | alert::status_notification; }
        peer_connected_alert(const net_identifier& np, const md4_hash& hash,
                            bool bActive) : peer_alert(np, hash),
                                            m_active(bActive)
        {}

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new peer_connected_alert(*this));
        }

        virtual std::string message() const { return std::string("peer connected alert"); }
        virtual char const* what() const { return "peer connected alert"; }
        bool m_active;
    };

    struct peer_disconnected_alert : public peer_alert
    {
        virtual int category() const { return static_category | alert::status_notification; }
        peer_disconnected_alert(const net_identifier& np,
                const md4_hash& hash,
                const error_code& ec) : peer_alert(np, hash), m_ec(ec)
        {}

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new peer_disconnected_alert(*this));
        }

        virtual std::string message() const { return std::string("peer disconnected alert"); }
        virtual char const* what() const { return "peer disconnected alert"; }
        error_code m_ec;
    };

    struct peer_message_alert : peer_alert
    {
        peer_message_alert(const net_identifier& np, const md4_hash& hash, const std::string& strMessage) :
            peer_alert(np, hash), m_strMessage(strMessage)
        {}

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new peer_message_alert(*this));
        }

        virtual std::string message() const { return std::string("peer message"); }
        virtual char const* what() const { return "peer notification"; }

       std::string m_strMessage;
    };

    struct peer_captcha_request_alert : peer_alert
    {
        peer_captcha_request_alert(const net_identifier& np, const md4_hash& hash, const std::vector<unsigned char>& captcha) :
            peer_alert(np, hash), m_captcha(captcha)
        {}

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new peer_captcha_request_alert(*this));
        }

        virtual std::string message() const { return std::string("peer captcha request"); }
        virtual char const* what() const { return "peer captcha request"; }

        std::vector<unsigned char>  m_captcha;
    };

    struct peer_captcha_result_alert : peer_alert
    {
        peer_captcha_result_alert(const net_identifier& np, const md4_hash& hash, boost::uint8_t nResult) :
            peer_alert(np, hash), m_nResult(nResult)
        {}

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new peer_captcha_result_alert(*this));
        }

        virtual std::string message() const { return std::string("peer captcha result"); }
        virtual char const* what() const { return "peer captcha result"; }

        boost::uint8_t  m_nResult;
    };

    struct shared_files_access_denied : peer_alert
    {
        shared_files_access_denied(const net_identifier& np, const md4_hash& hash) :
            peer_alert(np, hash)
        {}
        virtual int category() const { return static_category; }

        virtual std::string message() const { return "shared files access denied"; }
        virtual char const* what() const { return "shared files access denied"; }

        virtual std::auto_ptr<alert> clone() const
        {
            return (std::auto_ptr<alert>(new shared_files_access_denied(*this)));
        }
    };

    struct added_transfer_alert : alert
    {
        const static int static_category = alert::status_notification;

        added_transfer_alert(const transfer_handle& h) : m_handle(h) {}

        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new added_transfer_alert(*this));
        }

        virtual std::string message() const { return std::string("added transfer"); }
        virtual char const* what() const { return "added transfer"; }

        transfer_handle m_handle;
    };

    struct paused_transfer_alert : alert
    {
        const static int static_category = alert::status_notification;

        paused_transfer_alert(const transfer_handle& h) : m_handle(h) {}

        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new paused_transfer_alert(*this));
        }

        virtual std::string message() const { return std::string("paused transfer"); }
        virtual char const* what() const { return "paused transfer"; }

        transfer_handle m_handle;
    };

    struct resumed_transfer_alert : alert
    {
        const static int static_category = alert::status_notification;

        resumed_transfer_alert(const transfer_handle& h) : m_handle(h) {}

        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new resumed_transfer_alert(*this));
        }

        virtual std::string message() const { return std::string("resumed transfer"); }
        virtual char const* what() const { return "resumed transfer"; }

        transfer_handle m_handle;
    };

    struct deleted_transfer_alert : alert
    {
        const static int static_category = alert::status_notification;

        deleted_transfer_alert(const md4_hash& hash): m_hash(hash) {}

        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new deleted_transfer_alert(*this));
        }

        virtual std::string message() const { return std::string("deleted transfer"); }
        virtual char const* what() const { return "deleted transfer"; }

        md4_hash m_hash;
    };

    struct file_renamed_alert : alert
    {
        const static int static_category = alert::status_notification;

        file_renamed_alert(const transfer_handle& h, const std::string& name):
            m_handle(h), m_name(name) {}

        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new file_renamed_alert(*this));
        }

        virtual std::string message() const { return std::string("renamed file"); }
        virtual char const* what() const { return "renamed file"; }

        transfer_handle m_handle;
        std::string m_name;
    };

    struct file_rename_failed_alert : alert
    {
        const static int static_category = alert::status_notification;

        file_rename_failed_alert(const transfer_handle& h, const error_code& error):
            m_handle(h), m_error(error) {}

        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new file_rename_failed_alert(*this));
        }

        virtual std::string message() const { return std::string("rename failed transfer"); }
        virtual char const* what() const { return "rename failed transfer"; }

        transfer_handle m_handle;
        error_code m_error;
    };

    struct deleted_file_alert : alert
    {
        const static int static_category = alert::status_notification;

        deleted_file_alert(const transfer_handle& h, const md4_hash& hash):
            m_handle(h), m_hash(hash) {}

        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new deleted_file_alert(*this));
        }

        virtual std::string message() const { return std::string("deleted file"); }
        virtual char const* what() const { return "deleted file"; }

        transfer_handle m_handle;
        md4_hash m_hash;
    };

    struct delete_failed_transfer_alert : alert
    {
        const static int static_category = alert::status_notification;

        delete_failed_transfer_alert(const transfer_handle& h, const error_code& error):
            m_handle(h), m_error(error) {}

        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new delete_failed_transfer_alert(*this));
        }

        virtual std::string message() const { return std::string("delete failed transfer"); }
        virtual char const* what() const { return "delete failed transfer"; }

        transfer_handle m_handle;
        error_code m_error;
    };

    struct state_changed_alert : alert
    {
        const static int static_category = alert::status_notification;

        state_changed_alert(
            const transfer_handle& h, transfer_status::state_t new_state,
            transfer_status::state_t old_state):
            m_handle(h), m_new_state(new_state), m_old_state(old_state) {}

        virtual int category() const { return static_category; }

        virtual std::auto_ptr<alert> clone() const
        {
            return std::auto_ptr<alert>(new state_changed_alert(*this));
        }

        virtual std::string message() const { return std::string("changed transfer state"); }
        virtual char const* what() const { return "changed transfer state"; }

        transfer_handle m_handle;
        transfer_status::state_t m_new_state;
        transfer_status::state_t m_old_state;
    };

    struct transfer_alert: alert
    {
        transfer_alert(transfer_handle const& h)
            : m_handle(h)
        {}

        virtual std::string message() const
        { return m_handle.is_valid()?m_handle.hash().toString():" - "; }

        transfer_handle m_handle;
    };

    struct save_resume_data_alert: transfer_alert
    {
        save_resume_data_alert(boost::shared_ptr<entry> const& rd
            , transfer_handle const& h)
            : transfer_alert(h)
            , resume_data(rd)
        {}

        boost::shared_ptr<entry> resume_data;

        virtual std::auto_ptr<alert> clone() const
        { return std::auto_ptr<alert>(new save_resume_data_alert(*this)); }
        virtual char const* what() const { return "save resume data complete"; }
        const static int static_category = alert::storage_notification;
        virtual int category() const { return static_category; }

        virtual std::string message() const
        {
            return transfer_alert::message() + " resume data generated";
        }
    };

    struct save_resume_data_failed_alert: transfer_alert
    {
        save_resume_data_failed_alert(transfer_handle const& h
            , error_code const& e)
            : transfer_alert(h)
            , error(e)
        {
#ifndef TORRENT_NO_DEPRECATE
            msg = error.message();
#endif
        }

        error_code error;

#ifndef TORRENT_NO_DEPRECATE
        std::string msg;
#endif

        virtual std::auto_ptr<alert> clone() const
        { return std::auto_ptr<alert>(new save_resume_data_failed_alert(*this)); }
        virtual char const* what() const { return "save resume data failed"; }
        const static int static_category = alert::storage_notification
            | alert::error_notification;
        virtual int category() const { return static_category; }
        virtual std::string message() const
        {
            return transfer_alert::message() + " resume data was not generated: "
                + error.message();
        }
    };

    struct TORRENT_EXPORT fastresume_rejected_alert: transfer_alert
    {
        fastresume_rejected_alert(transfer_handle const& h
            , error_code const& e)
            : transfer_alert(h)
            , error(e)
        {
#ifndef TORRENT_NO_DEPRECATE
            msg = error.message();
#endif
        }

        error_code error;

#ifndef TORRENT_NO_DEPRECATE
        std::string msg;
#endif

        virtual std::auto_ptr<alert> clone() const
        { return std::auto_ptr<alert>(new fastresume_rejected_alert(*this)); }
        virtual char const* what() const { return "resume data rejected"; }
        const static int static_category = alert::status_notification | alert::error_notification;
        virtual int category() const { return static_category; }
        virtual std::string message() const
        {
            return transfer_alert::message() + " fast resume rejected: " + error.message();
        }
    };

    struct TORRENT_EXPORT file_error_alert: transfer_alert
    {
        file_error_alert(
            std::string const& f
            , transfer_handle const& h
            , error_code const& e)
            : transfer_alert(h)
            , file(f)
            , error(e)
        {
#ifndef TORRENT_NO_DEPRECATE
            msg = error.message();
#endif
        }

        std::string file;
        error_code error;

#ifndef TORRENT_NO_DEPRECATE
        std::string msg;
#endif

        virtual std::auto_ptr<alert> clone() const
        { return std::auto_ptr<alert>(new file_error_alert(*this)); }
        virtual char const* what() const { return "file error"; }
        const static int static_category = alert::status_notification
            | alert::error_notification
            | alert::storage_notification;
        virtual int category() const { return static_category; }
        virtual std::string message() const
        {
            return transfer_alert::message() + " file (" + file + ") error: "
                + error.message();
        }
    };

    struct transfer_checked_alert: transfer_alert
    {
        transfer_checked_alert(transfer_handle const& h)
            : transfer_alert(h)
        {}

        virtual std::auto_ptr<alert> clone() const
        { return std::auto_ptr<alert>(new transfer_checked_alert(*this)); }
        virtual char const* what() const { return "transfer checked"; }
        const static int static_category = alert::status_notification;
        virtual int category() const { return static_category; }
        virtual std::string message() const
        {
            return transfer_alert::message() + " checked";
        }
  };

}


#endif //__LIBED2K_ALERT_TYPES__
