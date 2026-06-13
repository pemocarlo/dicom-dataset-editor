include_guard()

if(NOT DICOM_EDITOR_DEVELOPER_MODE)
    return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
    message(AUTHOR_WARNING "Developer mode is intended for developers of DicomDatasetEditor")
endif()

option(DICOM_EDITOR_ENABLE_STRICT_WARNINGS "Enable strict compiler warnings and treat them as errors" ON)
option(DICOM_EDITOR_ENABLE_IWYU "Run include-what-you-use while compiling" OFF)

function(dicom_editor_absolutize out_var)
    set(paths ${ARGN})
    list(TRANSFORM paths PREPEND "${PROJECT_SOURCE_DIR}/")
    set(${out_var} "${paths}" PARENT_SCOPE)
endfunction()

find_program(DICOM_EDITOR_CLANG_FORMAT clang-format REQUIRED)
find_program(DICOM_EDITOR_CLANG_TIDY clang-tidy REQUIRED)
find_program(DICOM_EDITOR_CPPCHECK cppcheck REQUIRED)

set(DICOM_EDITOR_DEVELOPER_TARGETS dicom_editor_core dicom-dataset-editor)
if(BUILD_TESTING)
    list(APPEND DICOM_EDITOR_DEVELOPER_TARGETS dicom_editor_tests)
endif()

if(DICOM_EDITOR_ENABLE_STRICT_WARNINGS)
    include("${CMAKE_CURRENT_LIST_DIR}/CompilerWarnings.cmake")
    dicom_editor_enable_strict_warnings(${DICOM_EDITOR_DEVELOPER_TARGETS})
endif()

if(DICOM_EDITOR_ENABLE_IWYU)
    find_program(DICOM_EDITOR_IWYU_EXECUTABLE include-what-you-use REQUIRED)
    set_property(TARGET ${DICOM_EDITOR_DEVELOPER_TARGETS}
        PROPERTY CXX_INCLUDE_WHAT_YOU_USE
            "${DICOM_EDITOR_IWYU_EXECUTABLE};-Wno-unknown-warning-option;-Xiwyu;--error=1"
    )
endif()

dicom_editor_absolutize(
    DICOM_EDITOR_FORMAT_FILES
    ${DICOM_EDITOR_PUBLIC_HEADERS}
    ${DICOM_EDITOR_CORE_SOURCES}
    ${DICOM_EDITOR_UI_HEADERS}
    ${DICOM_EDITOR_UI_SOURCES}
    ${DICOM_EDITOR_TEST_SOURCES}
)
dicom_editor_absolutize(
    DICOM_EDITOR_LINT_FILES
    ${DICOM_EDITOR_CORE_SOURCES}
    ${DICOM_EDITOR_UI_SOURCES}
    ${DICOM_EDITOR_TEST_SOURCES}
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
        -p=${PROJECT_BINARY_DIR}
        ${DICOM_EDITOR_LINT_FILES}
    COMMENT "Linting C++ sources"
    VERBATIM
)

add_custom_target(cppcheck
    COMMAND ${DICOM_EDITOR_CPPCHECK}
        --project=${PROJECT_BINARY_DIR}/compile_commands.json
        --enable=warning,style,performance,portability
        --error-exitcode=1
        --inline-suppr
        --quiet
    COMMENT "Running cppcheck"
    VERBATIM
)

if(CMAKE_EXPORT_COMPILE_COMMANDS)
    if(CMAKE_GENERATOR MATCHES "Makefiles|Ninja")
        file(CREATE_LINK
            "${PROJECT_BINARY_DIR}/compile_commands.json"
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
