#include <string>
#include <cryptopp/config.h>
#ifdef WIN32
#include <windows.h>
#endif

// include config.h for byte definition
// special functions for encrypt and decrypt passwords
// it is simple port of Authorisation from emule is mod
// used for backward compatibility
namespace is_crypto
{
// it is simple ported code - define windows types
#ifndef WIN32
    typedef unsigned int DWORD;
    typedef unsigned int UINT;
#endif

void CreateIV(byte *src, int len_src, byte *iv, int len_iv);
void CreateKey(byte *key, int len_key);
void convert_number(int number, byte *array, int len_array);

std::string EncryptPasswd(const std::string& strPasswd, const std::string& strFilename);
std::string DecryptPasswd(const std::string& strEncpasswd, const std::string& strFilename);

};

