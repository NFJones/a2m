file(GLOB_RECURSE lib_SRC 
"*.h"
"*.cc")

file(GLOB_RECURSE lib_INCLUDE
"*.h")

add_library(njonesaudiolib-static STATIC ${lib_SRC})
add_library(njonesaudiolib SHARED ${lib_SRC})

set_target_properties(njonesaudiolib-static PROPERTIES OUTPUT_NAME njonesaudiolib)

install(FILES ${lib_INCLUDE}
        DESTINATION include/njones/lib)

install(TARGETS a2m a2m-static
        LIBRARY DESTINATION lib 
        ARCHIVE DESTINATION lib/static)
