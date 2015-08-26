if(DEFINED ENV{DATA_MODEL})
    set(bitness $ENV{DATA_MODEL})
else(DEFINED ENV{DATA_MODEL})
    message(FATAL_ERROR "DATA_MODEL is not set. Use export DATA_MODEL=32 or DATA_MODEL=64")
endif(DEFINED ENV{DATA_MODEL})

find_package(Boost 1.40 REQUIRED system thread random date_time unit_test_framework)
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

file(MAKE_DIRECTORY ${out_dir})

if(bitness MATCHES "64")
    set(cxx_flags "-m64 -D_LP64")
    set(l_flags "-m64")
else(bitness MATCHES "64")
    set(cxx_flags "-m32 -D_FILE_OFFSET_BITS=64")
    set(l_flags "-m32")
endif(bitness MATCHES "64")

if(PRODUCTION)
    set(cxx_flags "${cxx_flags} -O2")
    set(out_dir "${out_dir}/release")
else(DEFINED production)
    set(cxx_flags "${cxx_flags} -O0 -rdynamic -g -DDEBUG -D_DEBUG -DLIBED2K_DEBUG")
    set(out_dir "${out_dir}/debug")
endif(PRODUCTION)

set(cxx_flags "${cxx_flags} -Wall -Wno-long-long -Wpointer-arith -Wformat -pedantic -ansi -fPIC -std=c++98 -D__STDC_LIMIT_MACROS -DLIBED2K_USE_BOOST_DATE_TIME")

set(CMAKE_CXX_FLAGS ${cxx_flags})
if(DEFINED boost_home)
include_directories("${boost_home}/include")
set(l_flags "${l_flags} -L${boost_home}/lib")
endif(DEFINED boost_home)
include_directories(include)

message(STATUS "DATA_MODEL      = ${bitness}")
message(STATUS "BOOST_HOME      = ${boost_home}")
message(STATUS "PRODUCTION      = ${PRODUCTION}")
message(STATUS "BUILD_TESTS     = ${BUILD_TESTS}")
message(STATUS "BUILD_TOOLS	    = ${BUILD_TOOLS}")
message(STATUS "BUILD_SHARED    = ${BUILD_SHARED}")

