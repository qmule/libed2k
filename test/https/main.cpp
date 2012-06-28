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

class auth_handler
{
public:
    void on_auth(const std::string& strRes, const boost::system::error_code& error)
    {
        DBG("auth_handler::on_auth");

        if (error)
        {
            DBG("Error " << error.message());
        }

        DBG("Auth res: " << strRes);
    }
};

int main(int argc, char* argv[])
{
    LOGGER_INIT(LOG_ALL)

	try
	{
		if (argc < 3)
		{
			std::cerr << "Set login and password for test\n";
			return 1;
		}

		auth_handler ah;
		libed2k::auth_runner ar;

		ar.start("el.is74.ru", "auth.php", argv[1], argv[2], "0.5.6.7", boost::bind(&auth_handler::on_auth, &ah, _1, _2));
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
