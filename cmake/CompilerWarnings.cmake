function(dicom_editor_enable_strict_warnings)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(warnings
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Walloc-zero
            -Warray-bounds=2
            -Wcast-align=strict
            -Wcast-qual
            -Wconversion
            -Wdouble-promotion
            -Wduplicated-branches
            -Wduplicated-cond
            -Wformat=2
            -Wformat-overflow=2
            -Wformat-truncation=2
            -Wimplicit-fallthrough=5
            -Wlogical-op
            -Wmissing-declarations
            -Wmissing-include-dirs
            -Wnon-virtual-dtor
            -Wnull-dereference
            -Wold-style-cast
            -Woverloaded-virtual
            -Wpointer-arith
            -Wredundant-decls
            -Wshadow
            -Wsign-conversion
            -Wstringop-overflow=4
            -Wswitch-enum
            -Wundef
            -Wuseless-cast
            -Wvla
            -Wwrite-strings
            -Wzero-as-null-pointer-constant
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(warnings
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wcast-align
            -Wcast-qual
            -Wconversion
            -Wdouble-promotion
            -Wformat=2
            -Wmissing-declarations
            -Wnon-virtual-dtor
            -Wnull-dereference
            -Wold-style-cast
            -Woverloaded-virtual
            -Wshadow-all
            -Wsign-conversion
            -Wswitch-enum
            -Wundef
            -Wzero-as-null-pointer-constant
        )
    elseif(MSVC)
        set(warnings /W4 /WX /permissive-)
    else()
        message(WARNING "Strict warnings are not configured for ${CMAKE_CXX_COMPILER_ID}")
        return()
    endif()

    foreach(target IN LISTS ARGN)
        target_compile_options(${target} PRIVATE ${warnings})
    endforeach()
endfunction()
