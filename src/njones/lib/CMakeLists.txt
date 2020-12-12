file(GLOB_RECURSE lib_SRC 
"*.h"
"*.cc")

file(GLOB_RECURSE lib_INCLUDE
"*.h")

INSTALL(FILES ${lib_INCLUDE}
        DESTINATION include/njones/lib)
