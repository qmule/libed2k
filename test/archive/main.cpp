#include <stdint.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/stream.hpp>
#include "libed2k/archive.hpp"


using namespace libed2k::archive;
namespace io = boost::iostreams;


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
    friend class access;
    boost::uint16_t m_nA;
    boost::uint16_t m_nB;
    separated(boost::uint16_t nA, boost::uint16_t nB) : m_nA(nA), m_nB(nB)
    {
    }

    separated() : m_nA(0), m_nB(0)
    {}

    template<typename Archive>
    void save(Archive& ar) const
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

    LIBED2K_SERIALIZATION_SPLIT_MEMBER()
};

using boost::iostreams::file_sink;
using boost::iostreams::file_source;

int main()
{
    try
    {

        uint16_t data[] = {0x0F0A, 0x0405, 0000, 9000, 8000};


        char* dataPtr = (char*)&data;

        typedef boost::iostreams::basic_array_source<char> ASourceDevice;
        typedef boost::iostreams::basic_array_sink<char> ASinkDevice;

        typedef boost::iostreams::file_sink FSinkDevice;
        typedef boost::iostreams::file_source FSrcDevice;

        boost::iostreams::stream_buffer<ASourceDevice> array_source_buffer(dataPtr, sizeof(data));
        boost::iostreams::stream_buffer<FSinkDevice> file_sink_buffer("./some", BOOST_IOS::binary);
        std::istream i_stream(&array_source_buffer);
        std::ostream o_stream(&file_sink_buffer);



        boost::iostreams::stream< boost::iostreams::basic_array<char> > o_array_stream(dataPtr, sizeof(data));

        ed2k_oarchive aa(o_array_stream);
        some sa(100, 100);
        aa << sa;

        assert(data[0] == 100);
        assert(data[1] == 100);


        //boost::iostreams::stream<Device> str(dataPtr, sizeof(data));
        //boost::iostreams::stream_buffer<FDevice> fbuffer("./file.txt", std::ios_base::out);



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
        separated sep1(11,22);
        some s(5,6);
        some s3(100,200);
        ed2k_iarchive i_archive(i_stream);
        i_archive >> s;
        i_archive >> sep1;

        assert(sep1.m_nA == data[2]);
        assert(sep1.m_nB == data[3]);


        std::ostringstream sstream(std::ios_base::binary);
        ed2k_oarchive o_archive_s(sstream);
        o_archive_s << s3;
        std::cout << sstream.str().size() << std::endl;

        std::cout << "Binary: " << std::endl;
        std::cout << "BIN:  " << s.m_nFirst << ":" << s.m_nSecond << std::endl;

        //std::ostringstream s1(std::ios_base::binary);
        //s1 << s;
        std::string strSrc = "Source string";
        std::string strDst = "DstString";

        {
            std::ofstream ofs1("./some.txt", std::ios_base::binary);

            if (ofs1)
            {
               ed2k_oarchive ma(ofs1);
               ma << s;
               ma << strSrc;
            }
        }

        some some2(1,2);

        std::ifstream ifs1("./some.txt", std::ios_base::binary);
        {
            if (ifs1)
            {
                ed2k_iarchive myi(ifs1);
                myi >> some2;
                myi >> strDst;
                std::cout << "Restore " << some2.m_nFirst << ":" << some2.m_nSecond << std::endl;
            }

            assert(some2 == s);
            assert(strSrc == strDst);

            //ifs1.close();
            //ifs1.open("./some.txt"); //seekg(0, std::ios::beg);
            //boost::archive::binary_iarchive s_archive(ifs1, boost::archive::no_header);
            //s_archive >> some2;
            //std::cout << "Restore2 " << some2.m_nFirst << ":" << some2.m_nSecond << std::endl;

        }

        return 0;
        /*
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

