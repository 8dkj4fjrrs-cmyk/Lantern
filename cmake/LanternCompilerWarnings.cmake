# lantern_set_warnings(<target>)
#
# Applies a sensible, consistent warning level to one of OUR targets.
# Never call this on third-party targets: their warnings are not ours to fix.

function(lantern_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(LANTERN_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wshadow -Wconversion -Wsign-conversion
            -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual
            # C++20 designated initializers ({.title = "x"}) deliberately leave
            # other fields at their defaults; GCC would flag every such use.
            -Wno-missing-field-initializers)
        if(LANTERN_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
