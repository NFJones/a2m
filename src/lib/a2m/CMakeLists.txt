file(GLOB_RECURSE lib_SRC 
"*.h"
"*.cc")

file(GLOB_RECURSE lib_INCLUDE
"*.h")

include_directories("../")

add_library(a2m-static STATIC ${lib_SRC})
add_library(a2m SHARED ${lib_SRC})

set_target_properties(a2m-static PROPERTIES OUTPUT_NAME a2m)

install(TARGETS a2m  
        LIBRARY DESTINATION lib 
        ARCHIVE DESTINATION lib/static)

INSTALL(FILES ${lib_INCLUDE}
        DESTINATION include/a2m)
