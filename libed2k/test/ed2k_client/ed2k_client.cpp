// test console ed2k client

#include <iostream>
#include <boost/program_options.hpp>

#include "session.hpp"
#include "session_settings.hpp"
#include "log.hpp"

namespace po = boost::program_options;

int main(int argc, char* argv[])
{
    // Declare the supported options.
    po::options_description desc("ed2k_client options");
    desc.add_options()
        ("help", "produce help message")
        ("peer_port", po::value<int>(), "listen port for incoming peer connections")
        ("server", po::value<std::string>(), "ed2k server name");

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
    if (vm.count("peer_port"))
        settings.peer_port = vm["peer_port"].as<int>();

    init_logs();
    libed2k::session ses(print, "0.0.0.0", settings);

    std::cout << "---- libed2k_client started" << std::endl
              << "---- press q to exit" << std::endl;
    while (std::cin.get() != 'q');

    return 0;
}
