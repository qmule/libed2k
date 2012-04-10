#include <stdint.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/type_traits.hpp>
#include <boost/typeof/typeof.hpp>

namespace io = boost::iostreams;


// SFINAE test
template <typename T>
class has_save
{
    typedef char one;
    typedef long two;

    template <typename C> static one test( BOOST_TYPEOF(&C::helloworld) ) ;
    template <typename C> static two test(...);

public:
    enum { value = sizeof(test<T>(0)) == sizeof(char) };
//    static const bool value = (sizeof(test<T>(0)) == sizeof(char));
};

template <typename T>
class has_load
{
    typedef char one;
    typedef long two;

    template <typename C> static one test( BOOST_TYPEOF(&C::helloworld) ) ;
    template <typename C> static two test(...);

public:
//    static const bool value = (sizeof(test<T>(0)) == sizeof(char));
    enum { value = sizeof(test<T>(0)) == sizeof(char) };
};



class ed2k_oarchive
{
public:
    ed2k_oarchive(std::ostream& container) : m_container(container)
    {
		container.exceptions ( std::ios_base::failbit | std::ios_base::badbit | std::ios_base::eofbit);
    }

    template<typename T>
    ed2k_oarchive& operator<<(T& t)
    {
        serialize_impl(t);
        return *this;
    }

    template<typename T>
    ed2k_oarchive& operator&(T& t)
    {
        *this << t;
        return (*this);
    }

    ed2k_oarchive& operator<<(std::string& str)
    {
        uint16_t nSize = static_cast<uint16_t>(str.size());
        *this << nSize;
        std::cout << "write " << nSize << std::endl;
        raw_write(str.c_str(), str.size());
        return *this;
    }

    template<typename T>
    void raw_write(T p, size_t nSize)
    {
        m_container.write(p, nSize);
    }
private:
    // Fundamental
    template<typename T>
    inline void serialize_impl(T & val, typename boost::enable_if<boost::is_fundamental<T> >::type* dummy = 0)
    {
        raw_write(reinterpret_cast<char*>(&val), sizeof(T));
    }

    //Classes
    //template<typename T>
    //inline void serialize_impl(T & val, typename boost::enable_if<boost::is_class<T> >::type* dummy = 0)
    //{
    //    val.save(*this);
    //}

    //Classes
    template<typename T>
    inline void serialize_impl(T & val, typename boost::enable_if<boost::is_class<T> >::type* dummy = 0)
    {
        val.serialize(*this);
    }

    std::ostream& m_container;

};

class ed2k_iarchive
{
public:
    ed2k_iarchive(std::istream& container) : m_container(container)
    {
        container.exceptions ( std::ios_base::failbit | std::ios_base::badbit | std::ios_base::eofbit);
    }

    template<typename T>
    ed2k_iarchive& operator>>(T& t)
    {
        deserialize_impl(t);
        std::cout << "readed " << t << std::endl;
        return (*this);
    }

    template<typename T>
    ed2k_iarchive& operator&(T& t)
    {
        *this >> t;
        return (*this);
    }

    template<typename T>
    void raw_read(T t, size_t nSize)
    {
        m_container.read(t, nSize);
    }

    ed2k_iarchive& operator>>(std::string& str)
    {
        uint16_t nSize = static_cast<uint16_t>(str.size());
        *this >> nSize;
        std::cout << "read " << nSize << std::endl;

        if (nSize)
        {
			std::vector<char> v(nSize);

            raw_read(&v[0], nSize);
            str.assign(&v[0], nSize);
        }

        return *this;
    }
private:
    std::istream& m_container;

    template<typename T>
    inline void deserialize_impl(T & val, typename boost::enable_if<boost::is_fundamental<T> >::type* dummy = 0)
    {
        raw_read(reinterpret_cast<char*>(&val), sizeof(T));
    }

   
    //Classes
    template<typename T>
    inline void deserialize_impl(T & val, typename boost::enable_if<boost::is_class<T> >::type* dummy = 0)
    {
        val.serialize(*this);
        //deserialise_impl2<T>(val);
    }

/*
    template<typename T>
    inline void deserialise_impl2(T & val, typename boost::enable_if<typename has_load<T>::value >::type* dummy = 0)
    {
        val.save(*this);
    }

    template<typename T>
    inline void deserialise_impl2(T & val, typename boost::disable_if<typename has_load<T>::value >::type* dummy = 0)
    {
        val.serialize(*this);
    }
*/
    
};


struct some
{

    some() : m_nFirst(100), m_nSecond(100)
    {

    }

    some(uint16_t nFirst, uint16_t nSecond) : m_nFirst(nFirst), m_nSecond(nSecond)
    {
    }

    template<class Archive>
    void serialize(Archive & ar)
    {
        ar & m_nFirst;
        ar & m_nSecond;
    }
   
    uint16_t m_nFirst;
    uint16_t m_nSecond;

    bool operator==(const some& s)
    {
        return (m_nFirst == s.m_nFirst && m_nSecond == s.m_nSecond);
    }

};

struct separated
{
    boost::uint16_t m_nA;
    boost::uint16_t m_nB;
    separated(boost::uint16_t nA, boost::uint16_t nB) : m_nA(nA), m_nB(nB)
    {
    }

    separated() : m_nA(0), m_nB(0)
    {}

    template<typename Archive>
    void save(Archive& ar)
    {
        std::cout << "Call save\n";
       ar & m_nA;
       ar & m_nB;
    }  

    template<typename Archive>
    void load(Archive& ar)
    {
        std::cout << "Call load\n";
        ar & m_nA;
        ar & m_nB;
    }
};

std::ostream& operator<< (std::ostream& out, const some& s)
{
    out.write(reinterpret_cast<const char*>(&s.m_nFirst), sizeof(uint16_t));
    out.write(reinterpret_cast<const char*>(&s.m_nSecond), sizeof(uint16_t));
    return out;
}
//BOOST_CLASS_IMPLEMENTATION(some, boost::serialization::object_serializable);
//BOOST_CLASS_TRACKING(some, boost::serialization::track_never)

int main()
{
    try
    {
        //std::cout << has_helloworld<Hello>::value << std::endl;
        //std::cout << has_helloworld<Generic>::value << std::endl;

        uint16_t data[] = {1234, 5678, 0000};
        char* dataPtr = (char*)&data;

        typedef boost::iostreams::basic_array_source<char> Device;
        typedef boost::iostreams::file_sink FDevice;
        typedef boost::iostreams::file_source FSrcDevice;

        boost::iostreams::stream_buffer<Device> buffer(dataPtr, sizeof(data)); 
        std::istream i_stream(&buffer);
        boost::iostreams::stream<Device> str(dataPtr, sizeof(data));
        boost::iostreams::stream_buffer<FDevice> fbuffer("./file.txt", std::ios_base::out);

        //char chData[2];

        //boost::archive::binary_iarchive src_arch(str, boost::archive::no_header);

        //uint16_t n;
        //std::string sIn;
        //src_arch.load_binary(&n, sizeof(n));
        //src_arch.load(sIn);
        //std::cout << n << std::endl;


        //std::ofstream ofs("./file.txt", std::ios_base::binary);
       // boost::archive::binary_oarchive dst_archive(ofs, boost::archive::no_tracking | boost::archive::no_codecvt | boost::archive::no_header | boost::archive::no_xml_tag_checking);


        //std::ofstream st("./file.txt", ios::binary | ios::out);


        //uint16_t word1, word2;

        /*
        //while(str)
        word1 = src_arch.get();
        src_arch >> word2;
        std::cout << "eof" << std::endl;
        src_arch >> word1;
*/
        some s(5,6);
        ed2k_iarchive i_archive(i_stream);
        i_archive >> s;
        //i_archive >> s;

        std::cout << "Binary: " << std::endl;
        std::cout << "BIN:  " << s.m_nFirst << ":" << s.m_nSecond << std::endl;

        //std::ostringstream s1(std::ios_base::binary);
        //s1 << s;
        {
            std::ofstream ofs1("./some.txt", std::ios_base::binary);

            if (ofs1)
            {
               ed2k_oarchive ma(ofs1);
               ma << s;
            }
        }

        some some2(1,2);

        std::ifstream ifs1("./some.txt", std::ios_base::binary);
        {
            if (ifs1)
            {
                ed2k_iarchive myi(ifs1);
                myi >> some2;
                std::cout << "Restore " << some2.m_nFirst << ":" << some2.m_nSecond << std::endl;
            }

            assert(some2 == s);

            //ifs1.close();
            //ifs1.open("./some.txt"); //seekg(0, std::ios::beg);
            //boost::archive::binary_iarchive s_archive(ifs1, boost::archive::no_header);
            //s_archive >> some2;
            //std::cout << "Restore2 " << some2.m_nFirst << ":" << some2.m_nSecond << std::endl;

        }

        separated sp_out(100, 200);
        separated sp_in(9,8);
        
        {
            std::ofstream ofs1("./sep.txt", std::ios_base::binary);

            if (ofs1)
            {
               //ed2k_oarchive ma(ofs1);
               //ma << sp_out;
            }
        }

        {
            std::ifstream ifs1("./sep.txt", std::ios_base::binary);

            if (ifs1)
            {
               ed2k_iarchive ma(ifs1);
               //ma >> sp_in;
            }
        }

/*
        std::cout << s1.str().size() << " size \n";
        return 0;

        s.m_strData = "1234";
        //src_arch >> s;
        word1 = s.m_nFirst;
        word2 = s.m_nSecond;
        //src_arch >> word1;
        //src_arch >> word2;

        std::cout << "F: " << word1 << " S: " << word2 << std::endl;

        dst_archive << s;
        //dst_archive << word1;
        //dst_archive << word2;
        ofs.close();
        return 0;

        uint16_t w1, w2;
        boost::iostreams::stream_buffer<FSrcDevice> src_buffer("./file.txt", std::ios_base::in);
        std::ifstream ifs("./file.txt", std::ios_base::binary);
        boost::archive::binary_iarchive src_archive(ifs, boost::archive::no_header);
        src_archive >> w1;
        src_archive >> w2;
        ifs.close();

        assert(w1 == word1);
        assert(w2 == word2);

        s.m_nFirst = 0;
        s.m_nSecond = 0;

        std::stringstream sstream(std::ios_base::binary | std::ios_base::out | std::ios_base::in);
        //boost::archive::binary_oarchive out_archive(sstream, boost::archive::no_header);
        sstream << s;
        std::cout << sstream.str().size() << std::endl;
        sstream.seekg(0, std::ios::beg);
        //boost::archive::binary_iarchive in_archive(sstream, boost::archive::no_header);
        some s2;
        //sstream >> s2;
        //in_archive >> s2;
        std::cout <<  s.m_nFirst << ":" << s.m_nSecond << std::endl;
        std::cout <<  s2.m_nFirst << ":" << s2.m_nSecond << std::endl;
        assert(s == s2);
*/
    }
    catch(std::exception& e)
    {
           std::cout << e.what() << std::endl;

    }
    return 0;
}

