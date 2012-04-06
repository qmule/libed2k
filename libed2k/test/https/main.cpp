#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include "is_https_auth.hpp"


void on_auth(std::string& strRes, const boost::system::error_code& error)
{
    if (error)
    {
        std::cout << "Error " << error.message() << std::endl;
    }

    std::cout << "Auth res: " << strRes << std::endl;
}

int main(int argc, char* argv[])
{
	try
	{
		if (argc < 3)
		{
			std::cerr << "Set login and password for test\n";
			return 1;
		}
        
		boost::asio::io_service io_service;        
        libed2k::is_https_auth auth(io_service, on_auth);
		auth.requestAuth("el.is74.ru", "auth.php", argv[1], argv[2], "0.5.6.7");
		io_service.run();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

  return 0;
}
