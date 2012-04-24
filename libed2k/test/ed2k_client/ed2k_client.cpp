// test console ed2k client

#include <iostream>
#include <boost/program_options.hpp>

#include "session.hpp"
#include "session_settings.hpp"
#include "log.hpp"
#include "util.hpp"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

int main(int argc, char* argv[])
{
    // Declare the supported options.
    po::options_description desc("ed2k_client options");
    desc.add_options()
        ("server,s", po::value<std::string>(), "ed2k server name")
        ("dir,d", po::value<fs::path>(), "data directory")
        ("file,f", po::value<fs::path>(), "requested file name")
        ("peer_ip,i", po::value<std::string>(), "peer ip address")
        ("peer_port,p", po::value<int>(), "peer port")
        ("listen_port,l", po::value<int>(),
         "port for incoming peer connections (default 4662)")
        ("help,h", "produce help message");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
    } catch(po::error& poe) {
        std::cout << poe.what() << std::endl;
        return 1;
    }
    po::notify(vm);

    if (vm.count("help") || !vm.count("server"))
    {
        std::cout << desc << std::endl;
        return 1;
    }

    libed2k::fingerprint print;
    libed2k::session_settings settings;
    settings.server_hostname = vm["server"].as<std::string>();
    if (vm.count("listen_port"))
        settings.listen_port = vm["listen_port"].as<int>();

    init_logs();
    libed2k::session ses(print, "0.0.0.0", settings);

    if (vm.count("dir"))
    {
        fs::path dir = vm["dir"].as<fs::path>();
        ses.add_transfer_dir(dir);

        if (vm.count("file") && vm.count("peer_ip") && vm.count("peer_port"))
        {
            libed2k::add_transfer_params params;
            params.file_path = dir / vm["file"].as<fs::path>();
            params.info_hash = libed2k::hash_md4(params.file_path.filename());
            params.peer_list.push_back(
                libed2k::peer_entry(vm["peer_ip"].as<std::string>(),
                                    vm["peer_port"].as<int>()));
            params.seed_mode = false;
            ses.add_transfer(params);
        }
    }

    std::cout << "*** libed2k_client started ***" << std::endl
              << "*** press q to exit ***" << std::endl;
    while (std::cin.get() != 'q');

    return 0;
}
