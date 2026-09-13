include_guard()

if(NOT DICOM_EDITOR_DEVELOPER_MODE)
    return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
    message(AUTHOR_WARNING "Developer mode is intended for developers of DicomDatasetEditor")
endif()

option(DICOM_EDITOR_ENABLE_STRICT_WARNINGS "Enable strict compiler warnings and treat them as errors" ON)
option(DICOM_EDITOR_ENABLE_CLANG_FORMAT "Provide clang-format targets" ON)
option(DICOM_EDITOR_ENABLE_CLANG_TIDY "Provide the clang-tidy target" OFF)
option(DICOM_EDITOR_ENABLE_CPPCHECK "Provide the cppcheck target" OFF)
option(DICOM_EDITOR_ENABLE_IWYU "Run include-what-you-use while compiling" OFF)
option(DICOM_EDITOR_ENABLE_COVERAGE "Instrument project targets and provide a coverage target" OFF)

function(dicom_editor_absolutize out_var)
    set(paths ${ARGN})
    list(TRANSFORM paths PREPEND "${PROJECT_SOURCE_DIR}/")
    set(${out_var} "${paths}" PARENT_SCOPE)
endfunction()

if(DICOM_EDITOR_ENABLE_CLANG_FORMAT)
    find_program(DICOM_EDITOR_CLANG_FORMAT clang-format REQUIRED)
endif()
if(DICOM_EDITOR_ENABLE_CLANG_TIDY)
    find_program(DICOM_EDITOR_CLANG_TIDY clang-tidy REQUIRED)
endif()
if(DICOM_EDITOR_ENABLE_CPPCHECK)
    find_program(DICOM_EDITOR_CPPCHECK cppcheck REQUIRED)
endif()
if(DICOM_EDITOR_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "DICOM_EDITOR_ENABLE_COVERAGE currently requires GCC and lcov")
    endif()
    if(NOT BUILD_TESTING)
        message(FATAL_ERROR "DICOM_EDITOR_ENABLE_COVERAGE requires BUILD_TESTING=ON")
    endif()
    find_program(DICOM_EDITOR_LCOV lcov REQUIRED)
    find_program(DICOM_EDITOR_GENHTML genhtml REQUIRED)
    find_program(DICOM_EDITOR_GCOV gcov REQUIRED)
endif()

set(DICOM_EDITOR_DEVELOPER_TARGETS
    dicom_editor_core
    dicom_editor_application
    dicom_editor_fltk
    dicom-dataset-editor
)
if(BUILD_TESTING)
    list(APPEND DICOM_EDITOR_DEVELOPER_TARGETS dicom_editor_tests dicom_editor_gui_smoke_test)
endif()

if(DICOM_EDITOR_ENABLE_STRICT_WARNINGS)
    include("${CMAKE_CURRENT_LIST_DIR}/CompilerWarnings.cmake")
    dicom_editor_enable_strict_warnings(${DICOM_EDITOR_DEVELOPER_TARGETS})
endif()

if(DICOM_EDITOR_ENABLE_IWYU)
    find_program(DICOM_EDITOR_IWYU_EXECUTABLE include-what-you-use REQUIRED)
    if(MSVC)
        set(DICOM_EDITOR_IWYU_DRIVER_ARGUMENT --driver-mode=cl)
    else()
        set(DICOM_EDITOR_IWYU_DRIVER_ARGUMENT -Wno-unknown-warning-option)
    endif()
    set_property(TARGET ${DICOM_EDITOR_DEVELOPER_TARGETS}
        PROPERTY CXX_INCLUDE_WHAT_YOU_USE
            "${DICOM_EDITOR_IWYU_EXECUTABLE};${DICOM_EDITOR_IWYU_DRIVER_ARGUMENT};-Xiwyu;--error=1"
    )
endif()

if(DICOM_EDITOR_ENABLE_COVERAGE)
    set(DICOM_EDITOR_COVERAGE_LIBRARY_TARGETS
        dicom_editor_core
        dicom_editor_application
        dicom_editor_fltk
    )
    set(DICOM_EDITOR_COVERAGE_EXECUTABLE_TARGETS
        dicom-dataset-editor
        dicom_editor_tests
        dicom_editor_gui_smoke_test
    )

    foreach(target IN LISTS DICOM_EDITOR_COVERAGE_LIBRARY_TARGETS)
        target_compile_options(${target} PRIVATE --coverage)
    endforeach()
    foreach(target IN LISTS DICOM_EDITOR_COVERAGE_EXECUTABLE_TARGETS)
        target_compile_options(${target} PRIVATE --coverage)
        target_link_options(${target} PRIVATE --coverage)
    endforeach()

    set(DICOM_EDITOR_COVERAGE_OUTPUT_DIR
        "${PROJECT_BINARY_DIR}/coverage"
        CACHE PATH "Directory for generated lcov data and HTML coverage reports"
    )
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DICOM_EDITOR_COVERAGE_OUTPUT_DIR}"
        COMMAND ${CMAKE_CTEST_COMMAND}
            --test-dir "${PROJECT_BINARY_DIR}"
            --output-on-failure
            --no-tests=error
        COMMAND ${DICOM_EDITOR_LCOV}
            --capture
            --directory "${PROJECT_BINARY_DIR}"
            --gcov-tool "${DICOM_EDITOR_GCOV}"
            --output-file "${DICOM_EDITOR_COVERAGE_OUTPUT_DIR}/coverage.raw.info"
            --ignore-errors inconsistent
            --rc branch_coverage=1
        COMMAND ${DICOM_EDITOR_LCOV}
            --remove "${DICOM_EDITOR_COVERAGE_OUTPUT_DIR}/coverage.raw.info"
            "${PROJECT_SOURCE_DIR}/tests/*"
            "${PROJECT_BINARY_DIR}/*"
            "/usr/*"
            "*/conanhome/*"
            --output-file "${DICOM_EDITOR_COVERAGE_OUTPUT_DIR}/coverage.info"
            --ignore-errors inconsistent
            --rc branch_coverage=1
        COMMAND ${DICOM_EDITOR_GENHTML}
            "${DICOM_EDITOR_COVERAGE_OUTPUT_DIR}/coverage.info"
            --output-directory "${DICOM_EDITOR_COVERAGE_OUTPUT_DIR}/html"
            --branch-coverage
            --ignore-errors inconsistent,corrupt
            --title "DicomDatasetEditor coverage"
        DEPENDS ${DICOM_EDITOR_COVERAGE_EXECUTABLE_TARGETS}
        COMMENT "Running tests and generating the HTML coverage report"
        USES_TERMINAL
        VERBATIM
    )
endif()

dicom_editor_absolutize(
    DICOM_EDITOR_FORMAT_FILES
    ${DICOM_EDITOR_PUBLIC_HEADERS}
    ${DICOM_EDITOR_CORE_SOURCES}
    ${DICOM_EDITOR_APPLICATION_HEADERS}
    ${DICOM_EDITOR_APPLICATION_SOURCES}
    ${DICOM_EDITOR_FLTK_HEADERS}
    ${DICOM_EDITOR_FLTK_SOURCES}
    ${DICOM_EDITOR_APP_SOURCES}
    ${DICOM_EDITOR_TEST_SOURCES}
    ${DICOM_EDITOR_GUI_SMOKE_TEST_SOURCES}
)
dicom_editor_absolutize(
    DICOM_EDITOR_LINT_FILES
    ${DICOM_EDITOR_CORE_SOURCES}
    ${DICOM_EDITOR_APPLICATION_SOURCES}
    ${DICOM_EDITOR_FLTK_SOURCES}
    ${DICOM_EDITOR_APP_SOURCES}
    ${DICOM_EDITOR_TEST_SOURCES}
    ${DICOM_EDITOR_GUI_SMOKE_TEST_SOURCES}
)

if(DICOM_EDITOR_ENABLE_CLANG_FORMAT)
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
endif()

if(DICOM_EDITOR_ENABLE_CLANG_TIDY)
    add_custom_target(lint
        COMMAND ${DICOM_EDITOR_CLANG_TIDY}
            --quiet
            --warnings-as-errors=*
            -p=${PROJECT_BINARY_DIR}
            ${DICOM_EDITOR_LINT_FILES}
        COMMENT "Linting C++ sources"
        VERBATIM
    )
endif()

if(DICOM_EDITOR_ENABLE_CPPCHECK)
    add_custom_target(cppcheck
        COMMAND ${DICOM_EDITOR_CPPCHECK}
            --project=${PROJECT_BINARY_DIR}/compile_commands.json
            "-DTEST_CASE(...)=void catch_test_##__LINE__()"
            --enable=warning,style,performance,portability
            --error-exitcode=1
            --inline-suppr
            --quiet
        COMMENT "Running cppcheck"
        VERBATIM
    )
endif()

if(DICOM_EDITOR_ENABLE_VALGRIND)
    add_custom_target(valgrind
        COMMAND ${CMAKE_CTEST_COMMAND}
            --test-dir "${PROJECT_BINARY_DIR}"
            --build-config $<CONFIG>
            --test-action memcheck
            --output-on-failure
            --no-tests=error
        DEPENDS dicom_editor_tests dicom_editor_gui_smoke_test
        COMMENT "Running tests with Valgrind through CTest MemCheck"
        USES_TERMINAL
        VERBATIM
    )
endif()

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
