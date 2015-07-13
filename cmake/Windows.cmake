# Windows specific configuration
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    return()
endif(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")

set(cxx_definitions ${cxx_definitions} WITH_SHIPPED_GEOIP_H BOOST_LOG_DONOT_USE_WCHAR_T LIBED2K_USE_BOOST_DATE_TIME BOOST_ASIO_SEPARATE_COMPILATION BOOST_ASIO_ENABLE_CANCELIO _WIN32_WINNT=0x0501)

set(l_flags "${l_flags} ")
set(cxx_flags "${cxx_flags} ")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D LIBED2K_DEBUG")

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)


