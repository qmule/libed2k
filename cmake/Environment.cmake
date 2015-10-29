if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    MESSAGE( STATUS "64 bits compiler detected" )
    SET( EX_PLATFORM 64 )
    SET( EX_PLATFORM_NAME "x64" )
else( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    MESSAGE( STATUS "32 bits compiler detected" )
    SET( EX_PLATFORM 32 )
    SET( EX_PLATFORM_NAME "x86" )
endif( CMAKE_SIZEOF_VOID_P EQUAL 8 )

# set bitness
if(DEFINED ENV{DATA_MODEL})
    set(bitness $ENV{DATA_MODEL})
else(DEFINED ENV{DATA_MODEL})
    message( STATUS "DATA_MODEL is not set. Use platform ${EX_PLATFORM}")
    set(bitness ${EX_PLATFORM})
endif(DEFINED ENV{DATA_MODEL})

# Search packages for host system instead of packages for target system
# in case of cross compilation these macro should be defined by toolchain file
if(NOT COMMAND find_host_package)
  macro(find_host_package)
    find_package(${ARGN})
  endmacro()
endif()
if(NOT COMMAND find_host_program)
  macro(find_host_program)
    find_program(${ARGN})
  endmacro()
endif()

set(Boost_USE_STATIC_LIBS ON)
set(BOOST_LIBRARIES system thread random date_time)

if (BUILD_TESTS)
        set(BOOST_LIBRARIES ${BOOST_LIBRARIES} unit_test_framework)
endif()

file(MAKE_DIRECTORY ${out_dir})

if(PRODUCTION)
    set(out_dir "${out_dir}/release")
else(DEFINED production)
    set(out_dir "${out_dir}/debug")
endif(PRODUCTION)

set(CMAKE_CXX_FLAGS ${cxx_flags})
if(DEFINED boost_home)
include_directories("${boost_home}/include")
set(l_flags "${l_flags} -L${boost_home}/lib")
endif(DEFINED boost_home)
include_directories(include)

message(STATUS "DATA_MODEL      = ${bitness}")
message(STATUS "PRODUCTION      = ${PRODUCTION}")
message(STATUS "BUILD_TESTS     = ${BUILD_TESTS}")
message(STATUS "BUILD_TOOLS     = ${BUILD_TOOLS}")
message(STATUS "BUILD_SHARED    = ${BUILD_SHARED}")

