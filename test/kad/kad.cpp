#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread.hpp>
#include <boost/asio/error.hpp>

#include <libed2k/bencode.hpp>

#include "libed2k/log.hpp"
#include "libed2k/session.hpp"
#include "libed2k/session_settings.hpp"
#include "libed2k/util.hpp"
#include "libed2k/alert_types.hpp"
#include "libed2k/file.hpp"
#include "libed2k/search.hpp"
#include "libed2k/peer_connection_handle.hpp"
#include "libed2k/transfer_handle.hpp"
#include "libed2k/io_service.hpp"
#include "libed2k/server_connection.hpp"

using namespace libed2k;

std::deque<std::string> split_del(const std::string& input, char delimeter) {
    std::istringstream ss(input);
    std::deque<std::string> res;
    std::string part;
    while (std::getline(ss, part, delimeter)) res.push_back(part);
    return res;
}


void alerts_reader(const boost::system::error_code& ec, boost::asio::deadline_timer* pt, libed2k::session* ps)
{
    if (ec == boost::asio::error::operation_aborted)
    {
        return;
    }

    std::auto_ptr<alert> a = ps->pop_alert();

    while(a.get())
    {
        if (dynamic_cast<server_connection_initialized_alert*>(a.get()))
        {
            server_connection_initialized_alert* p = dynamic_cast<server_connection_initialized_alert*>(a.get());
            DBG("ALERT:  " << "server initalized: cid: " << p->client_id);
        }
        else if (dynamic_cast<server_name_resolved_alert*>(a.get()))
        {
            DBG("ALERT: server name was resolved: " << dynamic_cast<server_name_resolved_alert*>(a.get())->endpoint);
        }
        else if (dynamic_cast<server_status_alert*>(a.get()))
        {
            server_status_alert* p = dynamic_cast<server_status_alert*>(a.get());
            DBG("ALERT: server status: files count: " << p->files_count << " users count " << p->users_count);
        }
        else if (dynamic_cast<server_message_alert*>(a.get()))
        {
            server_message_alert* p = dynamic_cast<server_message_alert*>(a.get());
            DBG("ALERT: " << "msg: " << p->server_message);
        }
        else if (dynamic_cast<server_identity_alert*>(a.get()))
        {
            server_identity_alert* p = dynamic_cast<server_identity_alert*>(a.get());
            DBG("ALERT: server_identity_alert: " << p->server_hash << " name:  " << p->server_name << " descr: " << p->server_descr);
        }
        else if (shared_files_alert* p = dynamic_cast<shared_files_alert*>(a.get()))
        {           
            DBG("ALERT: RESULT: " << p->m_files.m_collection.size());
 
        }
        else if(dynamic_cast<peer_message_alert*>(a.get()))
        {
            peer_message_alert* p = dynamic_cast<peer_message_alert*>(a.get());
            DBG("ALERT: MSG: ADDR: " << int2ipstr(p->m_np.m_nIP) << " MSG " << p->m_strMessage);
        }
        else if (peer_disconnected_alert* p = dynamic_cast<peer_disconnected_alert*>(a.get()))
        {
            DBG("ALERT: peer disconnected: " << libed2k::int2ipstr(p->m_np.m_nIP));
        }
        else if (peer_captcha_request_alert* p = dynamic_cast<peer_captcha_request_alert*>(a.get()))
        {
            DBG("ALERT: captcha request ");
            FILE* fp = fopen("./captcha.bmp", "wb");

            if (fp)
            {
                fwrite(&p->m_captcha[0], 1, p->m_captcha.size(), fp);
                fclose(fp);
            }

        }
        else if (peer_captcha_result_alert* p = dynamic_cast<peer_captcha_result_alert*>(a.get()))
        {
            DBG("ALERT: captcha result " << p->m_nResult);
        }
        else if (peer_connected_alert* p = dynamic_cast<peer_connected_alert*>(a.get()))
        {
            DBG("ALERT: peer connected: " << int2ipstr(p->m_np.m_nIP) << " status: " << p->m_active);
        }
        else if (shared_files_access_denied* p = dynamic_cast<shared_files_access_denied*>(a.get()))
        {
            DBG("ALERT: peer denied access to shared files: " << int2ipstr(p->m_np.m_nIP));
        }
        else if (shared_directories_alert* p = dynamic_cast<shared_directories_alert*>(a.get()))
        {
            DBG("peer shared directories: " << int2ipstr(p->m_np.m_nIP) << " count: " << p->m_dirs.size());

            for (size_t n = 0; n < p->m_dirs.size(); ++n)
            {
                DBG("ALERT: DIR: " << p->m_dirs[n]);
            }
        }
        else if (dynamic_cast<save_resume_data_alert*>(a.get()))
        {
            DBG("ALERT: save_resume_data_alert");
        }
        else if (dynamic_cast<save_resume_data_failed_alert*>(a.get()))
        {
            DBG("ALERT: save_resume_data_failed_alert");
        }
        else if(transfer_params_alert* p = dynamic_cast<transfer_params_alert*>(a.get()))
        {
        	if (!p->m_ec)
        	{
        		DBG("ALERT: transfer_params_alert, add transfer for: " << p->m_atp.file_path);
        		ps->add_transfer(p->m_atp);
        	}
        }
        else
        {
            DBG("ALERT: Unknown alert: " << a.get()->message());
        }

        a = ps->pop_alert();
    }

    pt->expires_at(pt->expires_at() + boost::posix_time::seconds(3));
    pt->async_wait(boost::bind(&alerts_reader, boost::asio::placeholders::error, pt, ps));
}

// load nodes from standard emule nodes.dat file
bool load_nodes(const std::string& filename, libed2k::kad_nodes_dat& knd);

int main(int argc, char* argv[]) {
    LOGGER_INIT(LOG_ALL)
    std::cout << "---- kad started\n"
    << "---- press q/Q/quit to exit\n"
    << "---- press something other for process alerts \n"
    << "---- enter commands using command;param1;param2;....\n";
    
    libed2k::fingerprint print;
    libed2k::session_settings settings;
    settings.listen_port = 4668;
    libed2k::session ses(print, "0.0.0.0", settings);
    ses.set_alert_mask(alert::all_categories);    

    libed2k::io_service io;
    boost::asio::deadline_timer alerts_timer(io, boost::posix_time::seconds(3));
    boost::asio::deadline_timer fs_timer(io, boost::posix_time::minutes(3));
    alerts_timer.async_wait(boost::bind(alerts_reader, boost::asio::placeholders::error, &alerts_timer, &ses));
    boost::thread t(boost::bind(&libed2k::io_service::run, &io));

    std::string input;    

    while ((std::cin >> input)) {
        std::deque<std::string> command = split_del(input, ';');
        if (command.empty()) continue;
        if (command.at(0) == "quit" || command.at(0) == "q" || command.at(0) == "Q")  break;

        if (command.at(0) == "bootstrap") {
            if (command.size() < 3) {
                DBG("bootstrap command must have ip;udp_port additional information");
            }
            else {
                int udp_port = atoi(command.at(2).c_str());
                DBG("add dht router node " << command.at(1) << ":" << udp_port);
                ses.add_dht_router(std::make_pair(command.at(1), udp_port));
            }
        }
        else if (command.at(0) == "add") {
            if (command.size() < 3) {
                DBG("add command must have ip;udp_port additional information");
            }
            else {
                int udp_port = atoi(command.at(2).c_str());
                DBG("add dht node " << command.at(1) << ":" << udp_port);
                ses.add_dht_node(std::make_pair(command.at(1), udp_port), std::string("00000000000000000000000000000000"));
            }
        }
        else if (command.at(0) == "load") {
            if (command.size() < 2) {
                DBG("load command must have at least one specified nodes.dat file path");
            }
            else {
                for (size_t i = 1; i != command.size(); ++i) {
                    libed2k::kad_nodes_dat knd;
                    if (load_nodes(command.at(i), knd)) {
                        DBG("file " << command.at(i) << " load succesfully");
                        DBG("nodes.dat file version: " << knd.version);

                        for (size_t i = 0; i != knd.bootstrap_container.m_collection.size(); ++i) {
                            DBG("bootstrap " << knd.bootstrap_container.m_collection[i].kid.toString()
                                << " ip:" << libed2k::int2ipstr(knd.bootstrap_container.m_collection[i].address.address)
                                << " udp:" << knd.bootstrap_container.m_collection[i].address.udp_port
                                << " tcp:" << knd.bootstrap_container.m_collection[i].address.tcp_port);
                            ses.add_dht_node(std::make_pair(libed2k::int2ipstr(knd.bootstrap_container.m_collection[i].address.address),
                                knd.bootstrap_container.m_collection[i].address.udp_port), knd.bootstrap_container.m_collection[i].kid.toString());
                        }

                        for (std::list<kad_entry>::const_iterator itr = knd.contacts_container.begin(); itr != knd.contacts_container.end(); ++itr) {
                            DBG("nodes " << itr->kid.toString()
                                << " ip:" << libed2k::int2ipstr(itr->address.address)
                                << " udp:" << itr->address.udp_port
                                << " tcp:" << itr->address.tcp_port);
                                ses.add_dht_node(std::make_pair(libed2k::int2ipstr(itr->address.address), itr->address.udp_port), itr->kid.toString());
                        }
                    }
                    else {
                        DBG("file " << command.at(i) << " load failed");
                    }
                }
            }
        }
        else if (command.at(0) == "startdht") {
            ses.start_dht();
        }
        else if (command.at(0) == "stopdht") {
            ses.stop_dht();
        }
    }

    // cancel background operations and wait
    io.post(boost::bind(&boost::asio::deadline_timer::cancel, &alerts_timer));
    io.post(boost::bind(&boost::asio::deadline_timer::cancel, &fs_timer));
    t.join();
    return 0;
}

bool load_nodes(const std::string& filename, libed2k::kad_nodes_dat& knd) {
    DBG("File nodes: " << filename);

    std::ifstream fstream(filename.c_str(), std::ios_base::binary | std::ios_base::in);

    if (fstream) {
        using libed2k::kad_nodes_dat;
        using libed2k::kad_entry;
        libed2k::archive::ed2k_iarchive ifa(fstream);        

        try
        {
            ifa >> knd;       
            /*
            DBG("nodes.dat version:" << knd.version);

            for (size_t i = 0; i != knd.bootstrap_container.m_collection.size(); ++i) {
                DBG("bootstrap " << knd.bootstrap_container.m_collection[i].kid.toString()
                    << " ip:" << libed2k::int2ipstr(knd.bootstrap_container.m_collection[i].address.address)
                    << " udp:" << knd.bootstrap_container.m_collection[i].address.udp_port
                    << " tcp:" << knd.bootstrap_container.m_collection[i].address.tcp_port);
                    
            }

            for (std::list<kad_entry>::const_iterator itr = knd.contacts_container.begin(); itr != knd.contacts_container.end(); ++itr) {
                DBG("nodes " << itr->kid.toString()
                    << " ip:" << libed2k::int2ipstr(itr->address.address)
                    << " udp:" << itr->address.udp_port
                    << " tcp:" << itr->address.tcp_port);
                    
            }
            */
        }
        catch (libed2k_exception& e)
        {
            DBG("error on load nodes.dat " << e.what());
            return false;
        }
    }
    else {
        DBG("unable to open " << filename);
        return false;
    }

    return true;
}



