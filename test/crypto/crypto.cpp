#include <string>
#include <iostream>
#include <locale>
#include <libed2k/utf8.hpp>
#include "libed2k/log.hpp"
#include "libed2k/is_crypto.hpp"

const unsigned char chCryptKey[] = 
{ '\x30','\x82','\x01','\x07','\x02','\x01','\x00','\x30','\x0d','\x06','\x09','\x2a','\x86','\x48','\x86','\xf7','\x0d','\x01','\x01','\x01','\x05',
  '\x00','\x04','\x81','\xf2','\x30','\x81','\xef','\x02','\x01','\x00','\x02','\x31','\x00','\xe8','\x79','\xb7','\x89','\x8d','\x56','\x8a','\xe5','\x01','\x21','\xd9','\x15',
  '\x15','\xd5','\xa2','\x89','\xf7','\x53','\xff','\xc1','\x72','\x9c','\x23','\xdc','\x31','\xcb','\x4f','\xbf','\x33','\xca','\xa9','\x30','\xbf','\x01','\x3d','\x69','\xd9',
  '\xec','\xf3','\x41','\x7a','\xd2','\x9f','\xee','\x57','\xb0','\xfa','\x03','\x02','\x01','\x11','\x02','\x30','\x22','\x30','\x04','\x67','\x0d','\x41','\x6e','\xc7','\x52',
  '\xfd','\x72','\xbf','\x56','\x08','\xd4','\x23','\x59','\x13','\xe1','\xd8','\xae','\xf8','\xd8','\x18','\x92','\x64','\xf7','\x3a','\x71','\x13','\x42','\x5d','\xc3','\x2a',
  '\x5a','\xa2','\x8a','\x69','\x58','\x4d','\x4e','\x24','\x66','\xe4','\xa9','\xc1','\xac','\xfd','\x02','\x19','\x00','\xf4','\xe5','\x34','\xf1','\x68','\x60','\x19','\xd0',
  '\xfe','\xe0','\x61','\xf6','\x99','\x2f','\x44','\xc9','\xd5','\x26','\x1a','\xf4','\x22','\x51','\xe3','\x55','\x02','\x19','\x00','\xf3','\x04','\x56','\x73','\x97','\x4d',
  '\xff','\x48','\x91','\x00','\xd9','\x88','\x60','\x57','\xbc','\x69','\xf8','\xb4','\xfc','\x80','\xe6','\x3a','\x17','\xf7','\x02','\x18','\x73','\x3e','\xaf','\x80','\xa9',
  '\x96','\xa2','\xbc','\xb4','\x2d','\x5b','\x46','\xde','\xac','\xd5','\x13','\xaf','\x99','\x76','\x18','\x88','\x9f','\x01','\x91','\x02','\x18','\x47','\x79','\xbf','\x12',
  '\xf0','\x44','\x1d','\xe8','\x2a','\xa5','\xe5','\xa0','\x94','\xce','\x82','\xb5','\xc1','\x9e','\xa4','\x9e','\x61','\xd4','\xd9','\xdf','\x02','\x18','\x72','\xe3','\x3d',
  '\xc6','\x5b','\xd0','\x8e','\x6e','\x8a','\x30','\x2d','\x33','\x88','\x5b','\x3f','\xe0','\x36','\x88','\x2c','\x3d','\xec','\x54','\xce','\xef'};

const unsigned char chPasswd[] = 
{
'\x6A','\x00','\x58','\x00','\x37','\x00','\x72','\x00','\x2B','\x00','\x6E','\x00','\x49','\x00','\x55','\x00','\x4A','\x00','\x62','\x00','\x45','\x00','\x6D','\x00','\x54','\x00','\x51','\x00','\x73','\x00','\x66','\x00',
'\x30','\x00','\x44','\x00','\x53','\x00','\x48','\x00','\x57','\x00','\x73','\x00','\x77','\x00','\x59','\x00','\x31','\x00','\x61','\x00','\x46','\x00','\x35','\x00','\x4C','\x00','\x2F','\x00','\x50','\x00','\x6D','\x00',
'\x6F','\x00','\x6B','\x00','\x49','\x00','\x61','\x00','\x33','\x00','\x38','\x00','\x59','\x00','\x74','\x00','\x70','\x00','\x78','\x00','\x73','\x00','\x3D','\x00', '\x00', '\x00' }; //,'\x0A','\x00','\x00','\x00'};


int main(int argc, char* argv[])
{
    LOGGER_INIT(LOG_ALL)
    setlocale(LC_CTYPE, "");
    std::string strCrypto = "./crypto.dat";

    if (argc < 3)
    {
        std::cout << "Set key filename and password " << std::endl;
        return 1;
    }

    //FILE* fc = fopen(strCrypto.c_str(), "wb");
    //if (!fc)
    //{
    //    std::cout << "Unable to create crypt file" << std::endl;
    //    return 1;
    //}

    //fwrite(chCryptKey, 1, sizeof(chCryptKey), fc);
    //fclose(fc);


    DBG("Use key file: " << argv[1] << " and password: " << argv[2]);
	std::string strFilename = argv[1];
    //std::wstring strSrc;
    //strSrc.assign((wchar_t*)chPasswd, sizeof(chPasswd)/2);
    //strSrc = argv[2];

    //libtorrent::wchar_utf8(strSrc, strSrc2);
    std::string strSrc2 = argv[2];
    DBG("utf8 password: " << strSrc2);
    //char chBOM[] = {'\xEF', '\xBB', '\xBF'};

    //std::string strBom;
    //strBom.assign(chBOM, sizeof(chBOM));
    
    //std::string strEPassword = is_crypto::EncryptPasswd(argv[2], strFilename.c_str());
    std::string strPassword = is_crypto::DecryptPasswd(strSrc2, strFilename);

    //std::wstring strWideP;
    //libtorrent::utf8_wchar(strEPassword, strWideP);
    //FILE* fp = fopen("./dump.txt", "wb");

    //if (fp)
    //{
    //    fwrite(strWideP.c_str(), 2, strWideP.length(), fp);
    //    fclose(fp);
    //}

    //std::cout << "P: " << std::hex << strWideP.c_str() << std::endl;

	std::cout << "Decrypted password: " << strPassword << std::endl;
	//std::cout << "Encrypted password: " << strEPassword << std::endl;
	return 0;
};

