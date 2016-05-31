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
#include "libed2k/entry.hpp"

using namespace libed2k;

std::deque<std::string> split_del(const std::string& input, char delimeter) {
    std::istringstream ss(input);
    std::deque<std::string> res;
    std::string part;
    while (std::getline(ss, part, delimeter)) res.push_back(part);
    return res;
}

struct dht_keyword {
    libed2k::kad_id id;
    std::string name;
    libed2k::size_type size;
    unsigned int sources;
    dht_keyword(const libed2k::kad_info_entry& info) {
        id = info.hash;
        name = info.tags.getStringTagByNameId(libed2k::FT_FILENAME);
        size = info.tags.getIntTagByNameId(libed2k::FT_FILESIZE);
        sources = info.tags.getIntTagByNameId(libed2k::FT_SOURCES);
    }
};

std::deque<dht_keyword> dht_keywords;


void alerts_reader(const boost::system::error_code& ec, boost::asio::deadline_timer* pt, libed2k::session* ps)
{
    if (ec == boost::asio::error::operation_aborted) return;
    std::auto_ptr<alert> a = ps->pop_alert();
    while (a.get())
    {
        if (dynamic_cast<server_connection_initialized_alert*>(a.get()))
        {
            server_connection_initialized_alert* p = dynamic_cast<server_connection_initialized_alert*>(a.get());
            DBG("ALERT:  " << "server initalized: cid: " << p->client_id);
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
        else if (dht_keyword_search_result_alert* p = dynamic_cast<dht_keyword_search_result_alert*>(a.get()))
        {
            for (std::deque<libed2k::kad_info_entry>::const_iterator itr = p->m_entries.begin(); itr != p->m_entries.end(); ++itr) {
                dht_keywords.push_back(dht_keyword(*itr));
                DBG("search keyword added << " << dht_keywords.size() << ":" << dht_keywords.back().name << " << sources:" << dht_keywords.back().sources);                
            }
        }        
        
        a = ps->pop_alert();
    }
    
    pt->expires_at(pt->expires_at() + boost::posix_time::seconds(3));
    pt->async_wait(boost::bind(&alerts_reader, boost::asio::placeholders::error, pt, ps));
}

// load nodes from standard emule nodes.dat file
bool load_nodes(const std::string& filename, libed2k::kad_nodes_dat& knd);

libed2k::entry load_dht_state();
void save_dht_state(const libed2k::entry& e);


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
    alerts_timer.async_wait(boost::bind(alerts_reader, boost::asio::placeholders::error, &alerts_timer, &ses));
    boost::thread t(boost::bind(&libed2k::io_service::run, &io));
    std::string input;

    while ((std::cin >> input)) {
        std::deque<std::string> command = split_del(input, ';');
        if (command.empty()) continue;
        if (command.at(0) == "quit" || command.at(0) == "q" || command.at(0) == "Q")  break;

        if (command.at(0) == "find") {
            for (size_t i = 1; i != command.size(); ++i) {
                ses.find_keyword(command.at(i));
            }
        }
        else if (command.at(0) == "upnp") {
            DBG("forward ports using UPnP");
            ses.start_upnp();            
        }
        else if (command.at(0) == "natpmp") {
            DBG("forward ports using NatPmP");
            ses.stop_natpmp();
        }
        else if (command.at(0) == "bootstrap") {
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
            ses.start_dht(load_dht_state());
        }
        else if (command.at(0) == "stopdht") {
            save_dht_state(ses.dht_state());
            ses.stop_dht();
        }
        else if (command.at(0) == "download") {
            if (command.size() < 2) {
                DBG("command download must have one parameter - index of search entry");
            }
            else {
                int index = atoi(command.at(1).c_str());
                if (index >= 0 && index < dht_keywords.size()) {
                    libed2k::add_transfer_params params;
                    dht_keyword k = dht_keywords.at(index);
                    params.file_hash = k.id;
                    params.file_path = k.name;                    
                    params.file_size = k.size;
                    ses.add_transfer(params);
                    DBG("download " << k.name << " started");
                }
            }
        }
        else if (command.at(0) == "print") {
            size_t i = 0;
            for (std::deque<dht_keyword>::const_iterator itr = dht_keywords.begin(); itr != dht_keywords.end(); ++itr) {
                DBG(std::setw(3) << i++ << " " << itr->name << " size " << itr->size << " sources " << itr->sources);
            }
        }
    }

    // avoid assert on save empty entry
    libed2k::entry e = ses.dht_state();
    if (e.type() == libed2k::entry::dictionary_t) save_dht_state(ses.dht_state());

    io.post(boost::bind(&boost::asio::deadline_timer::cancel, &alerts_timer));
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

libed2k::entry load_dht_state() {
    libed2k::entry e;
    std::ifstream fs("dht_state.dat", std::ios_base::binary);
    if (fs) {
        std::noskipws(fs);
        std::string content((std::istreambuf_iterator<char>(fs)),
            (std::istreambuf_iterator<char>()));
        e = libed2k::bdecode(content.c_str(), content.c_str() + content.size());
    }

    return e;
}


void save_dht_state(const libed2k::entry& e) {
    std::ofstream fs("dht_state.dat", std::ios_base::binary);
    if (fs) {
        std::noskipws(fs);
        std::vector<char> out;
        libed2k::bencode(std::back_inserter(out), e);
        std::copy(out.begin(), out.end(), std::ostreambuf_iterator<char>(fs));
    }
}



