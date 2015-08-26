## Boost libraries

find_package(Boost 1.40 REQUIRED system thread random date_time unit_test_framework)
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

file(MAKE_DIRECTORY ${out_dir})

if(PRODUCTION)
    set(cxx_flags "${cxx_flags} -O2")
    set(out_dir "${out_dir}/release")
else(DEFINED production)
    set(cxx_flags "${cxx_flags} -O0 -rdynamic -g")
    set(out_dir "${out_dir}/debug")
	#set(cxx_definitions "${cxx_definitions} DEBUG _DEBUG LIBED2K_DEBUG")
endif(PRODUCTION)

include_directories(include)

message(STATUS "DATA_MODEL      = ${bitness}")
message(STATUS "BOOST_HOME      = ${boost_home}")
message(STATUS "PRODUCTION      = ${PRODUCTION}")
message(STATUS "BUILD_TESTS     = ${BUILD_TESTS}")
message(STATUS "BUILD_TOOLS	    = ${BUILD_TOOLS}")
message(STATUS "BUILD_SHARED    = ${BUILD_SHARED}")

