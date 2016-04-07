#ifndef WIN32
#define BOOST_TEST_DYN_LINK
#endif

#include <string>

#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE Main
#endif

#include <fstream>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/stream.hpp>
#include "libed2k/archive.hpp"
#include "libed2k/hasher.hpp"
#include "libed2k/packet_struct.hpp"
#include "libed2k/kademlia/node_id.hpp"
#include "common.hpp"

BOOST_AUTO_TEST_SUITE(test_kad)

BOOST_AUTO_TEST_CASE(test_kad_support_methods) {
    libed2k::md4_hash hash = libed2k::md4_hash::fromString("1AA8AFE3018B38D9B4D880D0683CCEB5");
    size_t i = 0;
    for (libed2k::md4_hash::const_iterator itr = hash.begin(); itr != hash.end(); ++itr) {
        BOOST_CHECK_EQUAL(*itr, hash[i++]);
    }

    BOOST_CHECK_EQUAL(i, libed2k::md4_hash::size);
}

typedef boost::iostreams::basic_array_source<char> ASourceDevice;
typedef boost::iostreams::basic_array_sink<char> ASinkDevice;

BOOST_AUTO_TEST_CASE(test_kad_identifier_serialization) {
    using libed2k::dht::node_id;
    node_id nodes[] = { libed2k::md4_hash::fromString("514d5f30f05328a05b94c140aa412fd3"),
        libed2k::md4_hash::fromString("59c729f19e6bc2ab269d99917bceb5a0"),
        libed2k::md4_hash::fromString("44d847c1c5e8d910d4200db8b464dbf4")
    };

    std::ostringstream sstream(std::ios_base::binary);
    libed2k::archive::ed2k_oarchive out_string_archive(sstream);

    for (size_t i = 0; i != sizeof(nodes) / sizeof(nodes[0]); ++i) {
        out_string_archive << nodes[i];
    }

    std::string buffer = sstream.str();

    boost::iostreams::stream_buffer<ASourceDevice> array_source_buffer2(buffer.c_str(), buffer.size());
    std::istream in_array_stream2(&array_source_buffer2);
    libed2k::archive::ed2k_iarchive in_array_archive2(in_array_stream2);
    size_t  i;
    for (i = 0; i != sizeof(nodes) / sizeof(nodes[0]); ++i) {
        node_id id;
        in_array_archive2 >> id;
        BOOST_CHECK_EQUAL(nodes[i], id);
    }

    BOOST_REQUIRE_EQUAL(3u, i);
}

BOOST_AUTO_TEST_CASE(test_kad_id_serialization) {
    const boost::uint32_t m_source_archive[4] = { 0x00000102, 0x0304, 0x0506, 0x0708 };
    const char* dataPtr = (const char*)&m_source_archive[0];

    boost::iostreams::stream_buffer<ASourceDevice> array_source_buffer(dataPtr, sizeof(m_source_archive));
    std::istream in_array_stream(&array_source_buffer);
    libed2k::archive::ed2k_iarchive in_array_archive(in_array_stream);
    libed2k::kad_id kid;
    in_array_archive >> kid;
    const boost::uint8_t tdata[16] = { '\x00', '\x00', '\x01', '\x02', '\x00', '\x00', '\x03', '\x04', '\x00', '\x00', '\x05', '\x06', '\x00', '\x00', '\x07', '\x08' };
    libed2k::kad_id tmpl_kid(tdata);
    BOOST_CHECK_EQUAL(tmpl_kid, kid);
    std::ostringstream sstream(std::ios_base::binary);
    libed2k::archive::ed2k_oarchive out_string_archive(sstream);
    out_string_archive << kid;
    BOOST_REQUIRE_EQUAL(libed2k::md4_hash::size, sstream.str().size());
    for (size_t i = 0; i < libed2k::md4_hash::size; i += 4) {
        BOOST_CHECK_EQUAL(m_source_archive[i / 4], *((boost::uint32_t*)(sstream.str().c_str() + i)));
    }
}

BOOST_AUTO_TEST_CASE(test_instantiation) {
    using namespace libed2k;
    kad_nodes_dat knd;
}

BOOST_AUTO_TEST_SUITE_END()

