include_guard()

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
        if(CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
            # The Visual Studio generator enables clang-cl's /Wall. Restore
            # the normal warning level before applying the project's strict
            # warning policy; /Wall also enables padding and experimental
            # unsafe-buffer diagnostics that are not enabled for the regular
            # Clang frontend.
            list(REMOVE_ITEM warnings -Wall)
            list(PREPEND warnings /W4)
            list(APPEND warnings -Wno-unsafe-buffer-usage)
        endif()
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
