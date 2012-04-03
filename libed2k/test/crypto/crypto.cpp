#include <string>
#include <iostream>
#include "Crypto.h"

int main(int argc, char* argv[])
{
	std::string strFilename = "./test.crypto";
	std::string strEPassword = IsCrypto::EncryptPasswd("password", strFilename);
	std::string strPassword = IsCrypto::DecryptPasswd(strEPassword, strFilename);

	std::cout << "Decrypted password: " << strPassword << std::endl;
	std::cout << "Encrypted password: " << strEPassword << std::endl;
	return 0;
};
