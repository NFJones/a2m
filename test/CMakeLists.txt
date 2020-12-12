file(GLOB_RECURSE test_SRC
"*.cc"
"*.h"
)

include_directories("./")
include_directories("../src")

set(CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR})
find_package(CxxTest)

if(CXXTEST_FOUND)
    include_directories(${CXXTEST_INCLUDE_DIR})
    enable_testing()

    CXXTEST_ADD_TEST(test-a2m test_audio_to_midi.cpp ${test_SRC})
	target_link_libraries(test-a2m a2m)
    add_dependencies(test-a2m a2m)
endif()

set_target_properties(test-a2m PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)

add_subdirectory(data)
