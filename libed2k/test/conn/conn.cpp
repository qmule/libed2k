#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "base_socket.hpp"
#include "log.hpp"
#include "session.hpp"
#include "session_settings.hpp"

using namespace libed2k;

int main(int argc, char* argv[])
{
    init_logs();

    if (argc < 3)
    {
        ERR("Set server host and port");
        return (1);
    }

    DBG("Server: " << argv[1] << " port: " << argv[2]);

    libed2k::fingerprint print;
    int listen_port = atoi(argv[2]);
    libed2k::session_settings settings;
    settings.server_hostname = argv[1];
    libed2k::session ses(print, listen_port, "0.0.0.0", settings);

    std::cout << "---- libed2k_client started" << std::endl
              << "---- press q to exit" << std::endl;
    while (std::cin.get() != 'q');

    return 0;
}

