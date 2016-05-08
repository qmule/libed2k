if (BUILD_TESTS)
        set(BOOST_LIBRARIES ${BOOST_LIBRARIES} unit_test_framework)
endif()

find_host_package(Boost 1.40 REQUIRED ${BOOST_LIBRARIES})
include_directories(${Boost_INCLUDE_DIR} )
link_directories(${Boost_LIBRARY_DIRS})

file(MAKE_DIRECTORY ${out_dir})


file(GLOB headers include/libed2k/*.hpp)
file(GLOB headers_kad include/libed2k/kademlia/*.hpp)

source_group(include FILES ${headers})
source_group(include\\kademlia FILES ${headers_kad})

file(GLOB_RECURSE sources src/*.cpp src/*.c)

if (BUILD_SHARED)
add_library(ed2k SHARED ${headers} ${headers_kad} ${sources})
else()
add_library(ed2k STATIC ${headers} ${headers_kad} ${sources})
endif()

set_target_properties(ed2k PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${out_dir} )
set_target_properties(ed2k PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${out_dir})
set_target_properties(ed2k PROPERTIES LINK_FLAGS ${l_flags})
set_target_properties(ed2k PROPERTIES COMPILE_FLAGS ${cxx_flags})

set(cxx_definitions ${cxx_definitions} LIBED2K_DHT_VERBOSE_LOGGING)
target_compile_definitions(ed2k PRIVATE ${cxx_definitions})

if (BUILD_TOOLS)
    foreach(ed2k_component conn archive dumper kad)
        file(GLOB_RECURSE component_headers test/${ed2k_component}/*hpp)
        file(GLOB_RECURSE component_sources test/${ed2k_component}/*cpp)
        add_executable(${ed2k_component} ${headers} ${component_headers} ${component_sources})
        set_target_properties(${ed2k_component} PROPERTIES COMPILE_FLAGS ${cxx_flags})
        set_target_properties(${ed2k_component} PROPERTIES LINK_FLAGS ${l_flags})
        set_target_properties(${ed2k_component} PROPERTIES RUNTIME_OUTPUT_DIRECTORY  ${out_dir})
        target_compile_definitions(${ed2k_component} PRIVATE ${cxx_definitions})
        target_link_libraries(${ed2k_component} ed2k)
        # link boost and system libraries
        TARGET_LINK_LIBRARIES(${ed2k_component} ${Boost_LIBRARIES} )
    endforeach(ed2k_component)
endif()

if (BUILD_TESTS)
    set(test_dir "${PROJECT_SOURCE_DIR}/unit")
    file(GLOB_RECURSE test_headers ${test_dir}/*hpp)
    file(GLOB_RECURSE test_sources ${test_dir}/*cpp)
    add_executable(run_tests ${headers} ${test_headers} ${test_sources})
    set_target_properties(run_tests PROPERTIES COMPILE_FLAGS ${cxx_flags})
    set_target_properties(run_tests PROPERTIES LINK_FLAGS ${l_flags})
    set_target_properties(run_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY  ${test_dir})
    target_compile_definitions(run_tests PRIVATE ${cxx_definitions})
    target_link_libraries(run_tests ed2k)
    TARGET_LINK_LIBRARIES(run_tests ${Boost_LIBRARIES} )
endif(BUILD_TESTS)
