#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include "libed2k/is_https_auth.hpp"
#include "libed2k/log.hpp"

void on_auth(const std::string& strRes, const boost::system::error_code& error)
{
    if (error)
    {
        std::cout << "Error " << error.message() << std::endl;
    }

    std::cout << "Auth res: " << strRes << std::endl;
}

int main(int argc, char* argv[])
{
    LOGGER_INIT()

	try
	{
		if (argc < 3)
		{
			std::cerr << "Set login and password for test\n";
			return 1;
		}

		libed2k::auth_runner ar;
		ar.start("el.is74.ru", "auth.php", argv[1], argv[2], "0.5.6.7", on_auth);
#ifdef WIN32
        Sleep(20000);
#else
		sleep(20);
#endif
		ar.start("el.is74.ru", "auth.php", argv[1], argv[2], "0.5.6.7", on_auth);
		//ar.start("el2.is74.ru", "auth.php", argv[1], argv[2], "0.5.6.7", on_auth);
		//ar.start("el.is74.ru", "auth.php", argv[1], argv[2], "0.5.6.7", on_auth);

	    while (std::cin.get() != 'q')
	    {
	    }

	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

  return 0;
}
