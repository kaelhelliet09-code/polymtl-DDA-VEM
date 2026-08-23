include_guard(GLOBAL)

function(dda_enable_strict_warnings target)
    set(options AS_ERRORS CONVERSIONS)
    cmake_parse_arguments(DDA_WARN "${options}" "" "" ${ARGN})

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            $<$<COMPILE_LANGUAGE:CXX>:/permissive->
        )
        if(DDA_WARN_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
        return()
    endif()

    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    if(DDA_WARN_CONVERSIONS)
        target_compile_options(${target} PRIVATE
            -Wconversion
            -Wsign-conversion
            -Wshadow=local
        )
    endif()

    if(DDA_WARN_AS_ERRORS)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()
