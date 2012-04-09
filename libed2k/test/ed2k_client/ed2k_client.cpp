// test console ed2k client

#include <iostream>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, char* argv[])
{
    // Declare the supported options.
    po::options_description desc("ed2k_client options");
    desc.add_options()
        ("help", "produce help message")
        ("mode", po::value<std::string>(), "client run mode");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
    } catch(boost::program_options::error& poe) {
        std::cout << poe.what() << std::endl;
        return 1;
    }
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 1;
    }

    if (vm.count("mode")) {
        std::cout << "Run mode is set to " 
                  << vm["mode"].as<std::string>() << std::endl;
    } else {
        std::cout << "Run mode was not set." << std::endl;
    }

    return 0;
}
