file(GLOB_RECURSE cli_SRC 
"*.h"
"*.cc")

include_directories("../lib")

add_executable(audio-to-midi ${cli_SRC})
target_link_libraries(audio-to-midi a2m)
