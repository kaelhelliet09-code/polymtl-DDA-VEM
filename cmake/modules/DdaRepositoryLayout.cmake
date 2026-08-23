include_guard(GLOBAL)

function(dda_validate_repository_layout)
    file(GLOB_RECURSE unexpected_generated_tree_cpp CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/Core/*.cpp"
        "${CMAKE_SOURCE_DIR}/Core/*.hpp"
        "${CMAKE_SOURCE_DIR}/USB_DEVICE/*.cpp"
        "${CMAKE_SOURCE_DIR}/USB_DEVICE/*.hpp"
    )

    if(unexpected_generated_tree_cpp)
        list(JOIN unexpected_generated_tree_cpp "\n  " unexpected_list)
        message(FATAL_ERROR
            "User-owned C++ must remain under Application/. Found:\n  "
            "${unexpected_list}"
        )
    endif()
endfunction()
