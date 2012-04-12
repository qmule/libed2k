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
#include "ctag.hpp"

BOOST_AUTO_TEST_SUITE(test_archive)

typedef boost::iostreams::basic_array_source<char> ASourceDevice;
typedef boost::iostreams::basic_array_sink<char> ASinkDevice;

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
    boost::uint16_t nStrDataSize = static_cast<boost::uint16_t>(strData.size());
    out_string_archive << nStrDataSize;
    out_string_archive << strData;
    BOOST_CHECK_EQUAL(sstream.str().length(), sizeof(SerialStruct) + sizeof(boost::uint16_t) + sizeof(bool) + sizeof(boost::uint16_t) + strData.size());
}


BOOST_AUTO_TEST_CASE(test_file_archive)
{
    SerialStruct ss1(1,2);
    SplittedStruct sp1(1,2,true,2);
    HideSplitted hs1(1,'B', 'C');
    std::string strData1 = "File test data";
    boost::uint8_t nDataLength1 = static_cast<boost::uint8_t>(strData1.size());

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
        ofa << nDataLength1;
        ofa << strData1;
        ofs.flush();
    }

    SerialStruct ss2(100,902);
    SplittedStruct sp2(1009,299,true,1212);
    HideSplitted hs2(341,'X', 'Y');
    std::string strData2 = "";
    boost::uint8_t nDataLength2 = 0;

    {
        std::ifstream ifs("./test.bin", std::ios_base::binary);
        libed2k::archive::ed2k_iarchive ifa(ifs);
        ifa >> ss2;
        ifa >> sp2;
        ifa >> hs2;
        ifa >> nDataLength2;
        strData2.resize(static_cast<size_t>(nDataLength2));
        ifa >> strData2;
    }

    BOOST_CHECK(ss2 == ss1);
    BOOST_CHECK(hs2 == hs1);
    BOOST_CHECK_EQUAL(sp2.m_nA,sp1.m_nA);
    BOOST_CHECK_EQUAL(sp2.m_nB,sp1.m_nB);
    BOOST_CHECK_EQUAL(sp2.m_nC,sp1.m_nC);
    BOOST_CHECK(strData2 == strData1);
}

BOOST_AUTO_TEST_CASE(test_ctag1)
{
    // it is test source array
    float fTemplate = 1292.54f;
    const char* pData = (const char*)&fTemplate;
    const boost::uint8_t m_source_archive[] =
            {   /*1 byte*/          static_cast<boost::uint8_t>(libed2k::tag::TAGTYPE_UINT8 | 0x80), '\x10', '\xED',
                /*2 bytes*/         static_cast<boost::uint8_t>(libed2k::tag::TAGTYPE_UINT16 | 0x80), '\x11', '\x0A', '\x0D',
                /*8 bytes*/         static_cast<boost::uint8_t>(libed2k::tag::TAGTYPE_UINT64), '\x04', '\x00', '\x30', '\x31', '\x32', '\x33', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07', '\x08',
                /*variable string*/ static_cast<boost::uint8_t>(libed2k::tag::TAGTYPE_STRING), '\x04', '\x00', 'A', 'B', 'C', 'D', '\x06', '\x00', 'S', 'T', 'R', 'I', 'N', 'G',
                /*defined string*/  static_cast<boost::uint8_t>(libed2k::tag::TAGTYPE_STR5), '\x04', '\x00', 'I', 'V', 'A', 'N', 'A', 'P', 'P', 'L', 'E',
                /*blob*/            static_cast<boost::uint8_t>(libed2k::tag::TAGTYPE_BLOB | 0x80), '\x0A', '\x03', '\x00', '\x00', '\x00', '\x0D', '\x0A', '\x0B',
                /*float*/           static_cast<boost::uint8_t>(libed2k::tag::TAGTYPE_FLOAT32 | 0x80), '\x15', *pData, *(pData+1), *(pData + 2), *(pData + 3),
                /*invalid blob*/    static_cast<boost::uint8_t>(libed2k::tag::TAGTYPE_BLOB | 0x80), '\x0A', '\xFF', '\xFF', '\xEE', '\xFF', '\x0D', '\x0A', '\x0B',};

    const char* dataPtr = (const char*)&m_source_archive[0];

    boost::iostreams::stream_buffer<ASourceDevice> array_source_buffer(dataPtr, sizeof(m_source_archive));
    std::istream in_array_stream(&array_source_buffer);
    libed2k::archive::ed2k_iarchive in_array_archive(in_array_stream);

    libed2k::tag t;     // test 1 byte unsigned tag
    libed2k::tag t2;    // test 2 byte unsigned tag
    libed2k::tag t3;    // test 8 byte unsigned tag + string name
    libed2k::tag t4;    // test string tag
    libed2k::tag t5;    // test predefined string type
    libed2k::tag t6;    // test blob type
    libed2k::tag t7;     // test float type
    libed2k::tag t8;    // test incorrect blob size

    in_array_archive >> t;
    BOOST_CHECK(t.isInt());
    boost::uint64_t nInt1 = 0xED;
    BOOST_CHECK_EQUAL(t.getInt(), nInt1);
    BOOST_CHECK_EQUAL(t.getNameID(), '\x10');

    in_array_archive >> t2;
    BOOST_CHECK(t2.isInt());
    boost::uint64_t nInt2 = 0x0D0A;
    BOOST_CHECK_EQUAL(t2.getInt(), nInt2);
    BOOST_CHECK_THROW(t2.getString(), libed2k::libed2k_exception);
    BOOST_CHECK_EQUAL(t2.getNameID(), '\x11');

    in_array_archive >> t3;
    BOOST_CHECK(t3.isInt());
    boost::uint64_t nInt3 = 0x0807060504030201UL;
    BOOST_CHECK_EQUAL(t3.getInt(), nInt3);
    BOOST_CHECK_EQUAL(t3.getNameID(), 0);
    BOOST_CHECK_EQUAL(t3.getName(), std::string("0123"));

    in_array_archive >> t4;
    BOOST_CHECK(t4.isStr());
    BOOST_CHECK_EQUAL(t4.getString(), std::string("STRING"));
    BOOST_CHECK_EQUAL(t4.getNameID(), 0);
    BOOST_CHECK_EQUAL(t4.getName(), std::string("ABCD"));

    in_array_archive >> t5;
    BOOST_CHECK(t5.isStr());
    BOOST_CHECK_THROW(t5.getInt(), libed2k::libed2k_exception);
    BOOST_CHECK_EQUAL(t5.getString(), std::string("APPLE"));
    BOOST_CHECK_EQUAL(t5.getNameID(), 0);
    BOOST_CHECK_EQUAL(t5.getName(), std::string("IVAN"));

    in_array_archive >> t6;
    BOOST_CHECK(t6.isBlob());
    BOOST_CHECK_EQUAL(t6.getBlob()[0], '\x0D');
    BOOST_CHECK_EQUAL(t6.getBlob()[1], '\x0A');
    BOOST_CHECK_EQUAL(t6.getBlob()[2], '\x0B');
    BOOST_CHECK_EQUAL(t6.getNameID(), '\x0A');

    in_array_archive >> t7;
    BOOST_CHECK(t7.isFloat());
    BOOST_CHECK_EQUAL(t7.getNameID(), '\x15');
    BOOST_CHECK_EQUAL(t7.getFloat(), fTemplate);

    BOOST_CHECK_THROW((in_array_archive >> t7), libed2k::libed2k_exception);

    // ok, prepare for save-restore
    std::stringstream sstream_out(std::ios::out | std::ios::in | std::ios::binary);
    libed2k::archive::ed2k_oarchive out_string_archive(sstream_out);

    out_string_archive << t;
    out_string_archive << t2;
    out_string_archive << t3;
    out_string_archive << t4;
    out_string_archive << t5;
    out_string_archive << t6;
    out_string_archive << t7;



    // go to begin
    libed2k::tag dt;     // test 1 byte unsigned tag
    libed2k::tag dt2;    // test 2 byte unsigned tag
    libed2k::tag dt3;    // test 8 byte unsigned tag + string name
    libed2k::tag dt4;    // test string tag
    libed2k::tag dt5;    // test predefined string type
    libed2k::tag dt6;    // test blob type
    libed2k::tag dt7;    // test float type

    sstream_out.flush();
    sstream_out.seekg(0, std::ios::beg);

    BOOST_REQUIRE(sstream_out.good());

    libed2k::archive::ed2k_iarchive in_string_archive(sstream_out);

    // restore
    in_string_archive >> dt;
    in_string_archive >> dt2;
    in_string_archive >> dt3;
    in_string_archive >> dt4;
    in_string_archive >> dt5;
    in_string_archive >> dt6;
    in_string_archive >> dt7;

    BOOST_CHECK(t  == dt);
    BOOST_CHECK(t2 == dt2);
    BOOST_CHECK(t3 == dt3);
    BOOST_CHECK(t4 == dt4);
    BOOST_CHECK(t5 == dt5);
    BOOST_CHECK(t6 == dt6);
    BOOST_CHECK(t7 == dt7);
}


BOOST_AUTO_TEST_SUITE_END()
