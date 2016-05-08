
cmake_minimum_required(VERSION 2.6)
project(libed2k)
set(libed2k_version 0.0.1)
set(out_dir "${PROJECT_SOURCE_DIR}/bin")

####### Build customization

# Override from command line "CMake -D<OPTION>=TRUE/FALSE/0/1/ON/OFF"
option (PRODUCTION  "Build project in production mode." FALSE)
option (BUILD_TESTS "Build unit tests." TRUE)
option (BUILD_TOOLS "Build tools." TRUE)
option (BUILD_SHARED "Build shared library." FALSE)

add_definitions(-DLIBED2K_UPNP_LOGGING=1)

include(cmake/Environment.cmake)
include(cmake/Linux.cmake)
include(cmake/Windows.cmake)
include(cmake/Android.cmake)
include(cmake/Targets.cmake)
