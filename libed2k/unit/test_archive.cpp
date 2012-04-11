#define BOOST_TEST_DYN_LINK

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <iostream>
#include <boost/test/unit_test.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/stream.hpp>
#include "archive.hpp"

BOOST_AUTO_TEST_SUITE(test_archive)

// symmetrical save/load
struct SerialStruct
{
    boost::uint16_t m_nA;
    boost::uint16_t m_nB;
    SerialStruct(boost::uint16_t nA, boost::uint16_t nB) : m_nA(nA), m_nB(nB){}

    template<typename Archive>
    void serialize(Archive& ar)
    {
        ar & m_nA;
        ar & m_nB;
    }

    bool operator ==(const SerialStruct& ss)
    {
        return (m_nA == ss.m_nA && m_nB == ss.m_nB);
    }
};

// asymmetrical save/load
struct SplittedStruct
{
    boost::uint16_t m_nA;
    boost::uint16_t m_nB;
    bool            m_bWithC;
    boost::uint16_t m_nC;
    SplittedStruct(boost::uint16_t nA, boost::uint16_t nB, bool bWithC, boost::uint16_t nC) : m_nA(nA), m_nB(nB), m_bWithC(bWithC), m_nC(nC)
    {

    }

    template<typename Archive>
    void save(Archive& ar) const
    {
        ar & m_nA;
        ar & m_nB;
        ar & m_nC;

    }

    template<typename Archive>
    void load(Archive& ar)
    {
        ar & m_nA;
        ar & m_nB;

        if (m_bWithC)
        {
            ar & m_nC;
        }
    }

    LIBED2K_SERIALIZATION_SPLIT_MEMBER()
};

// asymmetrical private members
class HideSplitted
{
public:
    friend class libed2k::archive::access;

    HideSplitted(boost::uint16_t nA, char cB, char cC) : m_nA(nA), m_cB(cB), m_cC(cC){}

    template<typename Archive>
    void save(Archive& ar) const
    {
        ar & m_nA;
    }

    template<typename Archive>
    void load(Archive& ar)
    {
        ar & m_nA;
        ar & m_cB;
        ar & m_cC;
    }

    LIBED2K_SERIALIZATION_SPLIT_MEMBER()

    boost::uint16_t getA() const
    {
        return (m_nA);
    }

    char getB() const
    {
        return (m_cB);
    }

    char getC() const
    {
        return (m_cC);
    }

    bool operator==(const HideSplitted& hs) const
    {
        return (m_nA == hs.m_nA && m_cB == hs.m_cB && m_cC == hs.m_cC);
    }
private:
    boost::uint16_t m_nA;
    char            m_cB;
    char            m_cC;
};

BOOST_AUTO_TEST_CASE(test_memory_archive)
{
    // it is test source array
    const boost::uint16_t m_source_archive[10] = {0x0102, 0x0304, 0x0506, 0x0708, 0x090A, 0x0B0C, 0x0D0E, 0x3040, 0x3020, 0xFFDD};
    const char* dataPtr = (const char*)&m_source_archive[0];
    SerialStruct ss_struct1(1,2);

    typedef boost::iostreams::basic_array_source<char> ASourceDevice;
    typedef boost::iostreams::basic_array_sink<char> ASinkDevice;
    boost::iostreams::stream_buffer<ASourceDevice> array_source_buffer(dataPtr, sizeof(m_source_archive));
    std::istream in_array_stream(&array_source_buffer);
    libed2k::archive::ed2k_iarchive in_array_archive(in_array_stream);
    in_array_archive >> ss_struct1;

    BOOST_CHECK_EQUAL(ss_struct1.m_nA, m_source_archive[0]);
    BOOST_CHECK_EQUAL(ss_struct1.m_nB, m_source_archive[1]);

    SplittedStruct sp_struct1(1,2, true, 100);
    in_array_archive >> sp_struct1;

    BOOST_CHECK_EQUAL(sp_struct1.m_nA, m_source_archive[2]);
    BOOST_CHECK_EQUAL(sp_struct1.m_nB, m_source_archive[3]);
    BOOST_CHECK_EQUAL(sp_struct1.m_nC, m_source_archive[4]);

    SplittedStruct sp_struct2(1,2, false, 100);
    in_array_archive >> sp_struct2;

    BOOST_CHECK_EQUAL(sp_struct2.m_nA, m_source_archive[5]);
    BOOST_CHECK_EQUAL(sp_struct2.m_nB, m_source_archive[6]);
    BOOST_CHECK_EQUAL(sp_struct2.m_nC, 100);

    HideSplitted hs1(1, 'A', 'B');
    in_array_archive >> hs1;

    BOOST_CHECK_EQUAL(hs1.getA(), m_source_archive[7]);
    BOOST_CHECK_EQUAL(hs1.getB(), ' ');
    BOOST_CHECK_EQUAL(hs1.getC(), '0');

    boost::uint32_t nLongData;
    BOOST_CHECK_THROW((in_array_archive >> nLongData), libed2k::libed2k_exception);

    // test arrays
    boost::iostreams::stream_buffer<ASourceDevice> array_source_buffer2(dataPtr, sizeof(m_source_archive));
    std::istream in_array_stream2(&array_source_buffer2);
    libed2k::archive::ed2k_iarchive in_array_archive2(in_array_stream2);

    boost::uint16_t vData[4] = {0,1,2,3};

    in_array_archive2.raw_read((char*)vData, sizeof(vData));

    BOOST_CHECK_EQUAL(m_source_archive[0], vData[0]);
    BOOST_CHECK_EQUAL(m_source_archive[1], vData[1]);
    BOOST_CHECK_EQUAL(m_source_archive[2], vData[2]);
    BOOST_CHECK_EQUAL(m_source_archive[3], vData[3]);
    in_array_stream2.seekg(4, std::ios_base::cur);
    uint16_t nData6;
    in_array_archive2 >> nData6;
    BOOST_CHECK_EQUAL(m_source_archive[6], nData6);

    // simple write operations
    std::ostringstream sstream(std::ios_base::binary);
    libed2k::archive::ed2k_oarchive out_string_archive(sstream);
    out_string_archive << ss_struct1;
    BOOST_CHECK_EQUAL(sstream.str().length(), sizeof(SerialStruct));
    out_string_archive << hs1;
    BOOST_CHECK_EQUAL(sstream.str().length(), sizeof(SerialStruct) + sizeof(boost::uint16_t));
    bool bData = false;
    out_string_archive << bData;
    std::string strData = "Simple";
    out_string_archive << strData;
    BOOST_CHECK_EQUAL(sstream.str().length(), sizeof(SerialStruct) + sizeof(boost::uint16_t) + sizeof(bool) + sizeof(boost::uint16_t) + strData.size());
}


BOOST_AUTO_TEST_CASE(test_file_archive)
{
    SerialStruct ss1(1,2);
    SplittedStruct sp1(1,2,true,2);
    HideSplitted hs1(1,'B', 'C');
    std::string strData1 = "File test data";

    {
        char B = 'B';
        char C = 'C';
        std::ofstream ofs("./test.bin", std::ios_base::binary);
        libed2k::archive::ed2k_oarchive ofa(ofs);
        ofa << ss1;
        ofa << sp1;
        ofa << hs1;
        ofa << B; // doesn't save by save call
        ofa << C; // doesn't save by save call
        ofa << strData1;
        ofs.flush();
    }

    SerialStruct ss2(100,90902);
    SplittedStruct sp2(1009,299,true,1212);
    HideSplitted hs2(341,'X', 'Y');
    std::string strData2 = "";

    {
        std::ifstream ifs("./test.bin", std::ios_base::binary);
        libed2k::archive::ed2k_iarchive ifa(ifs);
        ifa >> ss2;
        ifa >> sp2;
        ifa >> hs2;
        ifa >> strData2;
    }

    BOOST_CHECK(ss2 == ss1);
    BOOST_CHECK(hs2 == hs1);
    BOOST_CHECK_EQUAL(sp2.m_nA,sp1.m_nA);
    BOOST_CHECK_EQUAL(sp2.m_nB,sp1.m_nB);
    BOOST_CHECK_EQUAL(sp2.m_nC,sp1.m_nC);
    BOOST_CHECK(strData2 == strData1);


}


BOOST_AUTO_TEST_SUITE_END()
