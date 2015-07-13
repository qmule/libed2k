# Linux specific configuration
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    return()
endif(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux")

set(lib_variable "LD_LIBRARY_PATH")

# check compiler version
if(CMAKE_COMPILER_IS_GNUCXX)
    exec_program(
        ${CMAKE_CXX_COMPILER}
        ARGS                    --version
        OUTPUT_VARIABLE _compiler_output)
    string(REGEX REPLACE ".*([0-9]\\.[0-9]\\.[0-9]).*" "\\1"
        gcc_compiler_version ${_compiler_output})

    set(CMAKE_CXX_COMPILER_VERSION ${gcc_compiler_version})

    message(STATUS "C++ compiler version: ${gcc_compiler_version} [${CMAKE_CXX_COMPILER}]")
endif(CMAKE_COMPILER_IS_GNUCXX)

if(bitness MATCHES "64")
    set(cxx_flags "-m64 -D_LP64")
    set(l_flags "-m64")
else(bitness MATCHES "64")
    set(cxx_flags "-m32 -D_FILE_OFFSET_BITS=64")
    set(l_flags "-m32")
endif(bitness MATCHES "64")

if(DEFINED production)
    set(cxx_flags "${cxx_flags} -O2")
else(DEFINED production)
    set(cxx_flags "${cxx_flags} -DLIBED2K_DEBUG -O0 -rdynamic")
endif(DEFINED production)

if(DEFINED profiling)
    set(cxx_flags "${cxx_flags} -pg")
endif(DEFINED profiling)

set(c_flags "${cxx_flags} -fPIC")
set(cxx_flags "${cxx_flags} -Wall -Wno-long-long -Wpointer-arith -Wformat -Wno-unknown-pragmas -pedantic -ansi -fPIC -std=c++98 -D__STDC_LIMIT_MACROS")

if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "3.4.2")
    set(cxx_flags "${cxx_flags} -Wno-variadic-macros")
endif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "3.4.2")

set(l_flags "${l_flags} -lpthread")
set(cxx_definitions ${cxx_definitions} __STDC_LIMIT_MACROS LIBED2K_USE_BOOST_DATE_TIME)

if(DEFINED ENV{DATA_MODEL})
    set(bitness $ENV{DATA_MODEL})
else(DEFINED ENV{DATA_MODEL})
    message(FATAL_ERROR "DATA_MODEL is not set. Use export DATA_MODEL=32 or DATA_MODEL=64")
endif(DEFINED ENV{DATA_MODEL})

if(bitness MATCHES "64")
    set(cxx_flags "-m64 -D_LP64")
    set(l_flags "-m64")
else(bitness MATCHES "64")
    set(cxx_flags "-m32 -D_FILE_OFFSET_BITS=64")
    set(l_flags "-m32")
endif(bitness MATCHES "64")

