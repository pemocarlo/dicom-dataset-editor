cmake_minimum_required(VERSION 4.0)
include(cmake/Prelude.cmake)

project(
    DicomDatasetEditor
    VERSION 0.1.0
    DESCRIPTION "FLTK GUI for editing DICOM datasets"
    LANGUAGES CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
include(CTest)

# Only the Conan ClangCL profile needs this Visual Studio IntelliSense bridge.
if(
    CMAKE_GENERATOR_TOOLSET STREQUAL "ClangCL"
    AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
)
    include(cmake/VisualStudioClangIntelliSense.cmake)
endif()

option(DICOM_EDITOR_DEVELOPER_MODE "Enable developer tooling targets" OFF)
option(DICOM_EDITOR_BUILD_BENCHMARKS "Build synthetic performance benchmarks" OFF)
option(DICOM_EDITOR_ENABLE_VALGRIND "Provide a Valgrind CTest MemCheck target" OFF)
option(DICOM_EDITOR_BUILD_FLTK "Build the FLTK UI and executable" ON)
option(DICOM_EDITOR_BUILD_C_API "Build the optional pure C operations wrapper" OFF)

find_package(DCMTK CONFIG REQUIRED)
if(DICOM_EDITOR_BUILD_FLTK)
    find_package(fltk CONFIG REQUIRED)
    if(UNIX AND NOT APPLE)
        file(STRINGS "${fltk_INCLUDE_DIR}/FL/fl_config.h"
            DICOM_EDITOR_FLTK_WAYLAND_CONFIG
            REGEX "^#define FLTK_USE_WAYLAND 1"
        )
        if(DICOM_EDITOR_FLTK_WAYLAND_CONFIG)
            # ConanCenter FLTK 1.4.5 omits its auto-detected Wayland dependencies.
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(DICOM_EDITOR_FLTK_WAYLAND REQUIRED IMPORTED_TARGET
                pangocairo
                cairo
                wayland-client
                wayland-cursor
                xkbcommon
                dbus-1
                libdecor-0
            )
        endif()
    endif()
endif()

set(DICOM_VIEWER_OPERATIONS_PUBLIC_HEADERS
    include/dicom_viewer/operations.hpp
    include/dicom_editor/core/AttributeInput.hpp
    include/dicom_editor/core/DatasetViewModel.hpp
    include/dicom_editor/core/DicomDocument.hpp
    include/dicom_editor/core/DicomDictionary.hpp
    include/dicom_editor/core/DicomEditorService.hpp
    include/dicom_editor/core/DicomError.hpp
    include/dicom_editor/core/DicomNode.hpp
    include/dicom_editor/core/DicomPath.hpp
    include/dicom_editor/core/DicomTag.hpp
    include/dicom_editor/core/DicomWorkspace.hpp
    include/dicom_editor/core/StructuredReport.hpp
)
set(DICOM_VIEWER_OPERATIONS_SOURCES
    src/core/AttributeInput.cpp
    src/core/DatasetViewModel.cpp
    src/core/DicomDocument.cpp
    src/core/DicomDictionary.cpp
    src/core/DicomEditorService.cpp
    src/core/DicomNode.cpp
    src/core/DicomPath.cpp
    src/core/DicomWorkspace.cpp
    src/core/StructuredReport.cpp
)

set(DICOM_EDITOR_DCMTK_DICT_FILE "" CACHE FILEPATH "DCMTK dicom.dic to embed in the application")
if(NOT EXISTS "${DICOM_EDITOR_DCMTK_DICT_FILE}")
    message(FATAL_ERROR
        "DICOM_EDITOR_DCMTK_DICT_FILE must point to DCMTK's dicom.dic. "
        "Run the documented Conan install before configuring CMake."
    )
endif()
file(READ "${DICOM_EDITOR_DCMTK_DICT_FILE}" DICOM_EDITOR_DICTIONARY_TEXT)
string(FIND "${DICOM_EDITOR_DICTIONARY_TEXT}" ")DICOMDICT\"" DICOM_EDITOR_DICTIONARY_DELIMITER)
if(NOT DICOM_EDITOR_DICTIONARY_DELIMITER EQUAL -1)
    message(FATAL_ERROR "DCMTK dictionary contains the reserved embedded-resource delimiter")
endif()
configure_file(
    cmake/EmbeddedDictionary.cpp.in
    generated/EmbeddedDictionary.cpp
    @ONLY
)

add_library(dicom_viewer_operations)
add_library(DicomViewer::operations ALIAS dicom_viewer_operations)
set_target_properties(dicom_viewer_operations PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    EXPORT_NAME operations
    SOVERSION ${PROJECT_VERSION_MAJOR}
    VERSION ${PROJECT_VERSION}
    VISIBILITY_INLINES_HIDDEN YES
)
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/generated/include/dicom_viewer)
generate_export_header(dicom_viewer_operations
    EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/generated/include/dicom_viewer/export.hpp
    EXPORT_MACRO_NAME DICOM_VIEWER_OPERATIONS_EXPORT
    STATIC_DEFINE DICOM_VIEWER_OPERATIONS_STATIC_DEFINE
)
get_target_property(DICOM_VIEWER_OPERATIONS_TYPE dicom_viewer_operations TYPE)
if(DICOM_VIEWER_OPERATIONS_TYPE STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(dicom_viewer_operations
        PUBLIC DICOM_VIEWER_OPERATIONS_STATIC_DEFINE
    )
endif()
target_sources(dicom_viewer_operations
    PRIVATE
        ${DICOM_VIEWER_OPERATIONS_SOURCES}
        src/core/EmbeddedDictionary.hpp
        ${PROJECT_BINARY_DIR}/generated/EmbeddedDictionary.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS
            include
            ${PROJECT_BINARY_DIR}/generated/include
        FILES
            ${DICOM_VIEWER_OPERATIONS_PUBLIC_HEADERS}
            ${PROJECT_BINARY_DIR}/generated/include/dicom_viewer/export.hpp
)
set_source_files_properties(
    ${PROJECT_BINARY_DIR}/generated/EmbeddedDictionary.cpp
    PROPERTIES
        # The embedded dictionary is intentionally larger than Clang's
        # portable string-literal limit; keep this exception local to the
        # generated translation unit.
        COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:Clang>:-Wno-overlength-strings>"
)
target_include_directories(dicom_viewer_operations
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        src/core
)
target_link_libraries(dicom_viewer_operations PRIVATE dcmtk::dcmtk)

if(DICOM_EDITOR_BUILD_C_API)
    add_library(dicom_viewer_operations_c)
    add_library(DicomViewer::operations_c ALIAS dicom_viewer_operations_c)
    set_target_properties(dicom_viewer_operations_c PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        EXPORT_NAME operations_c
        VISIBILITY_INLINES_HIDDEN YES
    )
    generate_export_header(dicom_viewer_operations_c
        EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/generated/include/dicom_viewer/operations_c_export.h
        EXPORT_MACRO_NAME DICOM_VIEWER_OPERATIONS_C_EXPORT
        STATIC_DEFINE DICOM_VIEWER_OPERATIONS_C_STATIC_DEFINE
    )
    get_target_property(DICOM_VIEWER_OPERATIONS_C_TYPE dicom_viewer_operations_c TYPE)
    if(DICOM_VIEWER_OPERATIONS_C_TYPE STREQUAL "STATIC_LIBRARY")
        target_compile_definitions(dicom_viewer_operations_c
            PUBLIC DICOM_VIEWER_OPERATIONS_C_STATIC_DEFINE
        )
    endif()
    target_sources(dicom_viewer_operations_c
        PRIVATE
            src/c/operations_c.cpp
        PUBLIC
            FILE_SET HEADERS
            BASE_DIRS
                include
                ${PROJECT_BINARY_DIR}/generated/include
            FILES
                include/dicom_viewer/operations_c.h
                ${PROJECT_BINARY_DIR}/generated/include/dicom_viewer/operations_c_export.h
    )
    target_include_directories(dicom_viewer_operations_c
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_link_libraries(dicom_viewer_operations_c PRIVATE DicomViewer::operations)
endif()

set(DICOM_EDITOR_APPLICATION_HEADERS
    include/dicom_editor/application/EditorController.hpp
)
set(DICOM_EDITOR_APPLICATION_SOURCES
    src/application/EditorController.cpp
)
if(DICOM_EDITOR_BUILD_FLTK OR BUILD_TESTING)
    add_library(dicom_editor_application STATIC)
    add_library(DicomDatasetEditor::application ALIAS dicom_editor_application)
    target_sources(dicom_editor_application
        PRIVATE
            ${DICOM_EDITOR_APPLICATION_SOURCES}
        PUBLIC
            FILE_SET HEADERS
            BASE_DIRS include
            FILES
                ${DICOM_EDITOR_APPLICATION_HEADERS}
    )
    target_link_libraries(dicom_editor_application PUBLIC DicomViewer::operations)
endif()

if(DICOM_EDITOR_BUILD_FLTK)
    set(DICOM_EDITOR_FLTK_HEADERS
        src/gui/fltk/AttributeDialog.hpp
        src/gui/fltk/DatasetPanel.hpp
        src/gui/fltk/EditorWindow.hpp
        src/gui/fltk/FileTreePanel.hpp
        src/gui/fltk/PixelDataPanel.hpp
        src/gui/fltk/StructuredReportDialog.hpp
    )
    set(DICOM_EDITOR_FLTK_SOURCES
        src/gui/fltk/AttributeDialog.cpp
        src/gui/fltk/DatasetPanel.cpp
        src/gui/fltk/EditorWindow.cpp
        src/gui/fltk/EditorWindowDialogs.cpp
        src/gui/fltk/FileTreePanel.cpp
        src/gui/fltk/PixelDataPanel.cpp
        src/gui/fltk/StructuredReportDialog.cpp
    )
    add_library(dicom_editor_fltk STATIC)
    add_library(DicomDatasetEditor::fltk ALIAS dicom_editor_fltk)
    target_sources(dicom_editor_fltk
        PRIVATE
            ${DICOM_EDITOR_FLTK_HEADERS}
            ${DICOM_EDITOR_FLTK_SOURCES}
    )
    target_link_libraries(dicom_editor_fltk
        PUBLIC
            DicomDatasetEditor::application
            fltk::fltk
        PRIVATE
            $<$<TARGET_EXISTS:PkgConfig::DICOM_EDITOR_FLTK_WAYLAND>:PkgConfig::DICOM_EDITOR_FLTK_WAYLAND>
    )

    set(DICOM_EDITOR_APP_SOURCES
        src/gui/fltk/App.cpp
    )
    add_executable(dicom-dataset-editor WIN32)
    add_executable(DicomViewer::fltk_demo ALIAS dicom-dataset-editor)
    target_sources(dicom-dataset-editor
        PRIVATE
            ${DICOM_EDITOR_APP_SOURCES}
    )
    target_link_libraries(dicom-dataset-editor PRIVATE DicomDatasetEditor::fltk)
    if(APPLE)
        set_target_properties(dicom-dataset-editor PROPERTIES
            INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}"
        )
    elseif(UNIX)
        set_target_properties(dicom-dataset-editor PROPERTIES
            INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
        )
    endif()
endif()

install(TARGETS dicom_viewer_operations
    EXPORT DicomViewerOperationsTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(DICOM_EDITOR_BUILD_C_API)
    install(TARGETS dicom_viewer_operations_c
        EXPORT DicomViewerOperationsTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(DICOM_EDITOR_BUILD_FLTK)
    install(TARGETS dicom-dataset-editor)
endif()

install(EXPORT DicomViewerOperationsTargets
    FILE DicomViewerOperationsTargets.cmake
    NAMESPACE DicomViewer::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DicomViewer
)

configure_package_config_file(
    cmake/DicomViewerConfig.cmake.in
    ${PROJECT_BINARY_DIR}/DicomViewerConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DicomViewer
)
write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/DicomViewerConfigVersion.cmake
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${PROJECT_BINARY_DIR}/DicomViewerConfig.cmake
    ${PROJECT_BINARY_DIR}/DicomViewerConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DicomViewer
)

if(DICOM_EDITOR_ENABLE_VALGRIND)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "DICOM_EDITOR_ENABLE_VALGRIND is currently supported only on Linux")
    endif()
    find_program(MEMORYCHECK_COMMAND valgrind REQUIRED)
    set(MEMORYCHECK_TYPE Valgrind)
    set(MEMORYCHECK_COMMAND_OPTIONS
        "--tool=memcheck --error-exitcode=1 --leak-check=full --show-leak-kinds=definite,indirect,possible --errors-for-leak-kinds=definite,indirect"
    )
endif()

if(BUILD_TESTING)
    find_package(Catch2 3 CONFIG REQUIRED)
    set(DICOM_EDITOR_TEST_SOURCES tests/DicomEditorTests.cpp)
    if(DICOM_EDITOR_BUILD_FLTK)
        set(DICOM_EDITOR_GUI_SMOKE_TEST_SOURCES tests/GuiSmokeTest.cpp)
    endif()
    add_subdirectory(tests)
endif()

if(DICOM_EDITOR_BUILD_BENCHMARKS)
    add_executable(dicom_editor_benchmarks)
    target_sources(dicom_editor_benchmarks PRIVATE benchmarks/DicomEditorBenchmarks.cpp)
    target_include_directories(dicom_editor_benchmarks PRIVATE src/core)
    target_link_libraries(dicom_editor_benchmarks PRIVATE DicomViewer::operations dcmtk::dcmtk)
endif()

if(DICOM_EDITOR_DEVELOPER_MODE)
    include(cmake/DeveloperMode.cmake)
endif()
