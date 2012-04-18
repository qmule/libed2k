// test console ed2k client

#include <iostream>
#include <boost/program_options.hpp>

#include "session.hpp"
#include "session_settings.hpp"

namespace po = boost::program_options;

int main(int argc, char* argv[])
{
    // Declare the supported options.
    po::options_description desc("ed2k_client options");
    desc.add_options()
        ("help", "produce help message")
        ("mode", po::value<std::string>(), "client run mode")
        ("port", po::value<int>(), "listen port")
        ("logpath", po::value<std::string>(), "log path");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
    } catch(po::error& poe) {
        std::cout << poe.what() << std::endl;
        return 1;
    }
    po::notify(vm);

    if (vm.count("help") || !vm.count("mode") || !vm.count("port") ||
        !vm.count("logpath"))
    {
        std::cout << desc << std::endl;
        return 1;
    }

    std::cout << "Run mode is set to "
              << vm["mode"].as<std::string>() << std::endl;

    libed2k::fingerprint print;
    int port = vm["port"].as<int>();
    std::string logpath = vm["logpath"].as<std::string>();
    libed2k::session_settings settings;
    settings.server_hostname = "localhost";

    libed2k::session ses(print, port, "0.0.0.0", logpath, settings);

    std::cout << "---- libed2k_client started" << std::endl
              << "---- press q to exit" << std::endl;
    while (std::cin.get() != 'q');

    return 0;
}
