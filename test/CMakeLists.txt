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

    CXXTEST_ADD_TEST(test-audio-to-midi test_audio_to_midi.cpp ${test_SRC})
	target_link_libraries(test-audio-to-midi a2m)
    add_dependencies(test-audio-to-midi audio-to-midi)
endif()

add_subdirectory(data)
