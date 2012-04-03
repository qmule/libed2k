#include <string>
#include <iostream>
#include "is_crypto.hpp"

int main(int argc, char* argv[])
{
	std::string strFilename = "./xxx.txt";
	std::string strEPassword = is_crypto::EncryptPasswd("password", strFilename);
	std::string strPassword = is_crypto::DecryptPasswd(strEPassword, strFilename);

	std::cout << "Decrypted password: " << strPassword << std::endl;
	std::cout << "Encrypted password: " << strEPassword << std::endl;
	return 0;
};

