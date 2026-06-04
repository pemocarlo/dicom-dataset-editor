cmake_minimum_required(VERSION 4.0)

project(DicomDatasetEditor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(DCMTK CONFIG REQUIRED)
if(UNIX AND NOT APPLE)
    find_package(xorg-macros CONFIG REQUIRED)
    if(NOT TARGET xorg-macros::xorg-macros)
        add_library(xorg-macros::xorg-macros INTERFACE IMPORTED)
    endif()
endif()
find_package(wxWidgets CONFIG REQUIRED)

set(DICOM_EDITOR_CORE_SOURCES
    src/core/DicomDocument.cpp
    src/core/DicomEditorService.cpp
    src/core/DicomNode.cpp
    src/core/DicomPath.cpp
)

add_library(dicom_editor_core STATIC ${DICOM_EDITOR_CORE_SOURCES})
target_include_directories(dicom_editor_core PUBLIC include)
target_link_libraries(dicom_editor_core PUBLIC dcmtk::dcmtk)
if(EXISTS "${dcmtk_PACKAGE_FOLDER_RELEASE}/bin/share/dcmtk-3.7.0/dicom.dic")
    target_compile_definitions(dicom_editor_core
        PUBLIC DICOM_EDITOR_DCMTK_DICT_PATH="${dcmtk_PACKAGE_FOLDER_RELEASE}/bin/share/dcmtk-3.7.0/dicom.dic")
endif()

add_executable(dicom-dataset-editor
    src/gui/App.cpp
    src/gui/MainFrame.cpp
    src/gui/DatasetTreePanel.cpp
    src/gui/AttributeEditDialog.cpp
)
target_include_directories(dicom-dataset-editor PRIVATE include)
if(EXISTS "${fontconfig_PACKAGE_FOLDER_RELEASE}/res/etc/fonts")
    target_compile_definitions(dicom-dataset-editor
        PRIVATE DICOM_EDITOR_FONTCONFIG_PATH="${fontconfig_PACKAGE_FOLDER_RELEASE}/res/etc/fonts")
endif()
if(EXISTS "${gdk-pixbuf_PACKAGE_FOLDER_RELEASE}/bin/gdk-pixbuf-pixdata")
    target_compile_definitions(dicom-dataset-editor
        PRIVATE DICOM_EDITOR_GDK_PIXBUF_PIXDATA="${gdk-pixbuf_PACKAGE_FOLDER_RELEASE}/bin/gdk-pixbuf-pixdata")
endif()

if(TARGET wx::wxWidgets)
    set(DICOM_EDITOR_WX_TARGET wx::wxWidgets)
elseif(TARGET wxWidgets::wxWidgets)
    set(DICOM_EDITOR_WX_TARGET wxWidgets::wxWidgets)
elseif(TARGET wxWidgets::wx)
    set(DICOM_EDITOR_WX_TARGET wxWidgets::wx)
else()
    message(FATAL_ERROR "Unsupported wxWidgets Conan CMake target")
endif()

target_link_libraries(dicom-dataset-editor PRIVATE dicom_editor_core ${DICOM_EDITOR_WX_TARGET})

include(CTest)
if(BUILD_TESTING)
    add_executable(dicom_editor_tests tests/DicomEditorTests.cpp)
    target_link_libraries(dicom_editor_tests PRIVATE dicom_editor_core)
    add_test(NAME dicom_editor_tests COMMAND dicom_editor_tests)
endif()
