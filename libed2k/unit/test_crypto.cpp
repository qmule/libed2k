#define BOOST_TEST_DYN_LINK

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <boost/test/unit_test.hpp>
#include <string>
#include <boost/filesystem.hpp>
#include "is_crypto.hpp"

// test for encrypt / decrypt password 
struct test_crypto 
{
    test_crypto() : m_path("./crypto.key"), m_strSource("password") 
    {        
        m_strEncrypted = is_crypto::EncryptPasswd(m_strSource, m_path.string());
        m_strDecrypted = is_crypto::DecryptPasswd(m_strEncrypted, m_path.string());
    }
    
    
    bool isKeyFileExists() const
    {
        return (boost::filesystem::exists(m_path) && boost::filesystem::is_regular_file(m_path));
    }

    ~test_crypto() 
    {
        if (isKeyFileExists())
        {
            boost::filesystem::remove(m_path);
        }
    }
    
    boost::filesystem::path m_path;
    std::string m_strSource;
    std::string m_strEncrypted;
    std::string m_strDecrypted;
};

BOOST_FIXTURE_TEST_CASE(test_encrypt_decrypt, test_crypto) 
{
    BOOST_REQUIRE(isKeyFileExists());
    BOOST_CHECK_EQUAL(m_strSource, m_strDecrypted);
}

