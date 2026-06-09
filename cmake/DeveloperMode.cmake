find_program(DICOM_EDITOR_CLANG_FORMAT clang-format REQUIRED)
find_program(DICOM_EDITOR_CLANG_TIDY clang-tidy REQUIRED)

file(GLOB_RECURSE DICOM_EDITOR_FORMAT_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/*.hpp"
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/*.hpp"
    "${PROJECT_SOURCE_DIR}/tests/*.cpp"
)

file(GLOB_RECURSE DICOM_EDITOR_LINT_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/tests/*.cpp"
)

add_custom_target(format
    COMMAND ${DICOM_EDITOR_CLANG_FORMAT} -i ${DICOM_EDITOR_FORMAT_FILES}
    COMMENT "Formatting C++ sources"
    VERBATIM
)

add_custom_target(check-format
    COMMAND ${DICOM_EDITOR_CLANG_FORMAT} --dry-run --Werror ${DICOM_EDITOR_FORMAT_FILES}
    COMMENT "Checking C++ formatting"
    VERBATIM
)

add_custom_target(lint
    COMMAND ${DICOM_EDITOR_CLANG_TIDY}
        --quiet
        --warnings-as-errors=*
        -p=${CMAKE_BINARY_DIR}
        ${DICOM_EDITOR_LINT_FILES}
    COMMENT "Linting C++ sources"
    VERBATIM
)

if(DICOM_EDITOR_ENABLE_LSP)
    if(CMAKE_GENERATOR MATCHES "Makefiles|Ninja")
        file(CREATE_LINK
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${PROJECT_SOURCE_DIR}/compile_commands.json"
            SYMBOLIC
            RESULT DICOM_EDITOR_COMPILE_COMMANDS_LINK_RESULT
        )
        if(NOT DICOM_EDITOR_COMPILE_COMMANDS_LINK_RESULT STREQUAL "0")
            message(WARNING
                "Could not expose compile_commands.json at source root: "
                "${DICOM_EDITOR_COMPILE_COMMANDS_LINK_RESULT}"
            )
        endif()
    else()
        message(WARNING "CMAKE_EXPORT_COMPILE_COMMANDS is unsupported by ${CMAKE_GENERATOR}")
    endif()
endif()
