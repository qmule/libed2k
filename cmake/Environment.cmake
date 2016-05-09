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

## Boost libraries
#set(Boost_USE_STATIC_RUNTIME OFF)
set(Boost_USE_STATIC_LIBS ON)
set(BOOST_LIBRARIES system thread random date_time regex)

if (BUILD_TESTS)
        set(BOOST_LIBRARIES ${BOOST_LIBRARIES} unit_test_framework)
endif()

include_directories(include)

message(STATUS "DATA_MODEL      = ${bitness}")
message(STATUS "PRODUCTION      = ${PRODUCTION}")
message(STATUS "BUILD_TESTS     = ${BUILD_TESTS}")
message(STATUS "BUILD_TOOLS     = ${BUILD_TOOLS}")
message(STATUS "BUILD_SHARED    = ${BUILD_SHARED}")
if (DISABLE_DHT)
	message(STATUS "DHT	= disabled")
else()
	message(STATUS "DHT	= enabled")
endif()

