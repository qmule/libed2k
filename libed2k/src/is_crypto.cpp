#ifndef __IS_CRYPTO__
#define __IS_CRYPTO__

#include <time.h>
#include <cryptopp/blowfish.h>
#include <cryptopp/base64.h>
#include <cryptopp/files.h>
#include <cryptopp/modes.h>
#include <cryptopp/gzip.h>
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <cryptopp/md5.h>
#include "is_crypto.hpp"

namespace is_crypto
{

//функция преобразования числа в массив байтов заданной длинны по хитрому алгоритму :-)
//по какому значения не имеет, это всего лишь ключ
void convert_number(int number, byte *array, int len_array)
{
    const int SHIFT = 5;

    union
    {
        unsigned int i;
        unsigned char c[sizeof(int)];
    } diget;

    int tmpNumber = number;
    int countArray = 0;

    //заполняем начало массива на длину кратную sizeof(int)
    for(unsigned int i = 0; i < len_array/sizeof(int); i++)
    {
        //не знаю что из этой формулы выходит, придумывал на ходу
        diget.i = tmpNumber^((tmpNumber<<SHIFT)+i)&((tmpNumber<<(SHIFT*2))+i)|((tmpNumber<<(SHIFT*3))+i);
        tmpNumber = diget.i;
        for(int j=0; j<sizeof(int); j++){
            array[countArray++] = diget.c[j];
        }
    }

    //заполняем остаток массива
    diget.i = tmpNumber^(tmpNumber<<SHIFT)^(tmpNumber<<SHIFT*2);
    for(unsigned int i = 0; i < len_array%sizeof(int); i++)
    {
        array[countArray++] = diget.c[i%sizeof(int)];
    }
}

//функция формирования ключа
void CreateKey(byte *key, int len_key)
{
    memset( key, 0x00, len_key );

    //строим на основе серийника диска ключ
    DWORD VSNumber;

#ifdef WIN32
	// do not use wchar since we not need these values
    DWORD MCLength;
    DWORD FileSF;
    char NameBuffer[MAX_PATH];
    char SysNameBuffer[MAX_PATH];
    //получаем серииник тома
    if (!GetVolumeInformationA("C:\\",NameBuffer, sizeof(NameBuffer),
            &VSNumber,&MCLength,&FileSF,SysNameBuffer,sizeof(SysNameBuffer))){
        VSNumber = 1024; // если нам не удалось узнать серииник тома чтож, ставим заранее приготовленное число
    }
#else
    VSNumber = 1024;
#endif

    //на основе сероийника строим ключевую последовательность
    convert_number(VSNumber, key, len_key);
}

//функция формирования Функция формирования вектора инициализации
void CreateIV(byte *src, int len_src, byte *iv, int len_iv)
{
    memset( iv, 0x00, len_iv );

    for(int i=0; i<len_iv; i++){
        iv[i] = src[i%len_src];
    }
}

//функция кодирования сообщения по алгоритму AES
//ключ кодирования вычиляется путем сложения секретного слова (зашитого в функции) и логина
std::string EncryptPasswd(const std::string& strPasswd, const std::string& strFilename)
{
    namespace Weak = CryptoPP::Weak1;

    #define MAX_NUMBER_IN_FILE 10

    //преобразуем типы for what?
    //string spasswd = (string)StrToUtf8(passwd);
    //string snameFile = (string)StrToUtf8(nameFile);

    //создаём новый файл со случайным числом
    srand((UINT)time(NULL));

	// TODO - add file created test?
	std::fstream rand_file(strFilename.c_str(), std::ios::out| std::ios::trunc);

    for(int i=0;i<MAX_NUMBER_IN_FILE;i++)
    {
        rand_file<<rand();
    }

    rand_file.close();

    //берём хеш от файла
    Weak::MD5 hash;
    byte hashFile[Weak::MD5::DIGESTSIZE]; //output size of the buffer

    try
    {
        CryptoPP::FileSource f(strFilename.c_str(), true, new CryptoPP::HashFilter(hash, new CryptoPP::ArraySink(hashFile, Weak::MD5::DIGESTSIZE)));
    }
    catch(...)
    {
        memset( hashFile, 0x01, Weak::MD5::DIGESTSIZE );
    }

    // Key and IV setup
    byte key[ CryptoPP::Blowfish::DEFAULT_KEYLENGTH ],iv[ CryptoPP::Blowfish::BLOCKSIZE ];
    //создаём ключ
    CreateKey( key, CryptoPP::Blowfish::DEFAULT_KEYLENGTH);
    CreateIV(hashFile, Weak::MD5::DIGESTSIZE, iv, CryptoPP::Blowfish::BLOCKSIZE);

    std::string cipher;
    {
        CryptoPP::StringSink* sink = new CryptoPP::StringSink(cipher);
        CryptoPP::Base64Encoder* base64_enc = new CryptoPP::Base64Encoder(sink);
        CryptoPP::CBC_Mode<CryptoPP::Blowfish>::Encryption twofish(key, CryptoPP::Blowfish::DEFAULT_KEYLENGTH, iv);
        CryptoPP::StreamTransformationFilter* enc = new CryptoPP::StreamTransformationFilter(twofish, base64_enc);
        CryptoPP::Gzip *zip = new CryptoPP::Gzip(enc);
        CryptoPP::StringSource source(strPasswd, true, zip);
    }

    return (cipher);
}


//Функция декодирования пароля закодированного по алгоритму AES
std::string DecryptPasswd(const std::string& strEncpasswd, const std::string& strFilename)
{
    namespace Weak = CryptoPP::Weak1;

    //преобразуем типы for what?
    //string spasswd = (string)StrToUtf8(encpasswd);
    //string snameFile = (string)StrToUtf8(nameFile);

    Weak::MD5 hash;
    byte hashFile[Weak::MD5::DIGESTSIZE]; //output size of the buffer

    try
    {
        CryptoPP::FileSource f(strFilename.c_str(), true, new CryptoPP::HashFilter(hash, new CryptoPP::ArraySink(hashFile, Weak::MD5::DIGESTSIZE)));
    }
    catch(...)
    {
        memset( hashFile, 0x01, Weak::MD5::DIGESTSIZE );
    }

    // Key and IV setup
    byte key[ CryptoPP::Blowfish::DEFAULT_KEYLENGTH ],iv[ CryptoPP::Blowfish::BLOCKSIZE ];
    //создаём ключ
    CreateKey( key, CryptoPP::Blowfish::DEFAULT_KEYLENGTH);
    CreateIV(hashFile, Weak::MD5::DIGESTSIZE, iv, CryptoPP::Blowfish::BLOCKSIZE);

    std::string decipher;
    try {
        CryptoPP::StringSink *sink = new CryptoPP::StringSink(decipher);
        CryptoPP::Gunzip *unzip = new CryptoPP::Gunzip (sink);
        CryptoPP::CBC_Mode<CryptoPP::Blowfish>::Decryption twofish(key, CryptoPP::Blowfish::DEFAULT_KEYLENGTH, iv);
        CryptoPP::StreamTransformationFilter *dec = new CryptoPP::StreamTransformationFilter(twofish, unzip);
        CryptoPP::Base64Decoder *base64_dec = new CryptoPP::Base64Decoder(dec);
        CryptoPP::StringSource source(strEncpasswd, true, base64_dec);
    }
    catch (...)
    {
        decipher.clear();
    }

    ///return OptUtf8ToStr((std::stringA)decipher.c_str()); ?
    return (decipher);
}
};

#endif //__IS_CRYPTO__
