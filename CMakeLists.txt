cmake_minimum_required(VERSION 4.0)
include(cmake/Prelude.cmake)

project(
    DicomDatasetEditor
    VERSION 0.1.0
    DESCRIPTION "FLTK GUI for editing DICOM datasets"
    LANGUAGES CXX
)

option(DICOM_EDITOR_DEVELOPER_MODE "Enable developer tooling targets" OFF)

find_package(DCMTK CONFIG REQUIRED)
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

set(DICOM_EDITOR_PUBLIC_HEADERS
    include/dicom_editor/AttributeInput.hpp
    include/dicom_editor/DatasetViewModel.hpp
    include/dicom_editor/DicomDocument.hpp
    include/dicom_editor/DicomEditorService.hpp
    include/dicom_editor/DicomError.hpp
    include/dicom_editor/DicomNode.hpp
    include/dicom_editor/DicomPath.hpp
    include/dicom_editor/EditorController.hpp
    include/dicom_editor/RuntimePaths.hpp
)
set(DICOM_EDITOR_CORE_SOURCES
    src/core/AttributeInput.cpp
    src/core/DatasetViewModel.cpp
    src/core/DicomDocument.cpp
    src/core/DicomEditorService.cpp
    src/core/DicomNode.cpp
    src/core/DicomPath.cpp
    src/core/EditorController.cpp
)

set(DICOM_EDITOR_DCMTK_DICT_FILE "" CACHE FILEPATH "Path to DCMTK's dicom.dic")
if(NOT EXISTS "${DICOM_EDITOR_DCMTK_DICT_FILE}")
    message(FATAL_ERROR
        "DICOM_EDITOR_DCMTK_DICT_FILE must point to DCMTK's dicom.dic. "
        "Run the documented Conan install before configuring CMake."
    )
endif()

add_library(dicom_editor_core STATIC)
add_library(DicomDatasetEditor::core ALIAS dicom_editor_core)
target_sources(dicom_editor_core
    PRIVATE
        ${DICOM_EDITOR_CORE_SOURCES}
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            ${DICOM_EDITOR_PUBLIC_HEADERS}
)
target_link_libraries(dicom_editor_core PUBLIC dcmtk::dcmtk)
target_compile_definitions(dicom_editor_core
    PRIVATE
        DICOM_EDITOR_DCMTK_DICT_FILE="${DICOM_EDITOR_DCMTK_DICT_FILE}"
        DICOM_EDITOR_INSTALL_DATADIR="../share/dicom-dataset-editor"
)

set(DICOM_EDITOR_UI_HEADERS
    src/ui/AttributeDialog.hpp
    src/ui/DatasetPanel.hpp
    src/ui/EditorWindow.hpp
)
set(DICOM_EDITOR_UI_SOURCES
    src/ui/App.cpp
    src/ui/AttributeDialog.cpp
    src/ui/DatasetPanel.cpp
    src/ui/EditorWindow.cpp
)
set(DICOM_EDITOR_UI_LIBRARIES fltk::fltk)
if(TARGET PkgConfig::DICOM_EDITOR_FLTK_WAYLAND)
    list(APPEND DICOM_EDITOR_UI_LIBRARIES PkgConfig::DICOM_EDITOR_FLTK_WAYLAND)
endif()

add_executable(dicom-dataset-editor)
target_sources(dicom-dataset-editor
    PRIVATE
        ${DICOM_EDITOR_UI_HEADERS}
        ${DICOM_EDITOR_UI_SOURCES}
)
target_link_libraries(dicom-dataset-editor PRIVATE dicom_editor_core ${DICOM_EDITOR_UI_LIBRARIES})

install(TARGETS dicom-dataset-editor)
install(
    FILES "${DICOM_EDITOR_DCMTK_DICT_FILE}"
    DESTINATION share/dicom-dataset-editor/dcmtk
)

include(CTest)
if(BUILD_TESTING)
    set(DICOM_EDITOR_TEST_SOURCES tests/DicomEditorTests.cpp)
    add_executable(dicom_editor_tests)
    target_sources(dicom_editor_tests PRIVATE ${DICOM_EDITOR_TEST_SOURCES})
    target_link_libraries(dicom_editor_tests PRIVATE dicom_editor_core)
    add_test(NAME dicom_editor_tests COMMAND dicom_editor_tests)
endif()

if(DICOM_EDITOR_DEVELOPER_MODE)
    include(cmake/DeveloperMode.cmake)
endif()
