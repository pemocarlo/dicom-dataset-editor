cmake_minimum_required(VERSION 4.0)

project(DicomDatasetEditor VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DICOM_EDITOR_RELOCATABLE_INSTALL
    "Bundle non-system shared libraries and use relative runtime paths"
    OFF
)
option(DICOM_EDITOR_DEVELOPER_MODE "Enable developer tooling targets" OFF)
option(DICOM_EDITOR_ENABLE_LSP "Export compile commands and expose them at the source root" OFF)
option(DICOM_EDITOR_ENABLE_STRICT_WARNINGS "Enable strict compiler warnings and treat them as errors" OFF)
option(DICOM_EDITOR_ENABLE_IWYU "Run include-what-you-use while compiling" OFF)

if(DICOM_EDITOR_ENABLE_LSP)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

if(DICOM_EDITOR_ENABLE_IWYU)
    find_program(DICOM_EDITOR_IWYU_EXECUTABLE include-what-you-use REQUIRED)
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE
        "${DICOM_EDITOR_IWYU_EXECUTABLE};-Wno-unknown-warning-option;-Xiwyu;--error=1"
    )
endif()

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

set(DICOM_EDITOR_CORE_SOURCES
    src/core/AttributeInput.cpp
    src/core/DatasetViewModel.cpp
    src/core/DicomDocument.cpp
    src/core/DicomEditorService.cpp
    src/core/DicomNode.cpp
    src/core/DicomPath.cpp
    src/core/EditorController.cpp
)

add_library(dicom_editor_core STATIC ${DICOM_EDITOR_CORE_SOURCES})
target_include_directories(dicom_editor_core PUBLIC include)
target_link_libraries(dicom_editor_core PUBLIC dcmtk::dcmtk)
if(EXISTS "${dcmtk_PACKAGE_FOLDER_RELEASE}/bin/share/dcmtk-3.7.0/dicom.dic")
    target_compile_definitions(dicom_editor_core
        PUBLIC DICOM_EDITOR_DCMTK_DICT_PATH="${dcmtk_PACKAGE_FOLDER_RELEASE}/bin/share/dcmtk-3.7.0/dicom.dic")
endif()

set(DICOM_EDITOR_UI_SOURCES
    src/ui/App.cpp
    src/ui/EditorWindow.cpp
    src/ui/DatasetPanel.cpp
    src/ui/AttributeDialog.cpp
)
set(DICOM_EDITOR_UI_LIBRARIES fltk::fltk)
if(TARGET PkgConfig::DICOM_EDITOR_FLTK_WAYLAND)
    list(APPEND DICOM_EDITOR_UI_LIBRARIES PkgConfig::DICOM_EDITOR_FLTK_WAYLAND)
endif()

add_executable(dicom-dataset-editor ${DICOM_EDITOR_UI_SOURCES})
target_include_directories(dicom-dataset-editor PRIVATE include)
target_link_libraries(dicom-dataset-editor PRIVATE dicom_editor_core ${DICOM_EDITOR_UI_LIBRARIES})

if(DICOM_EDITOR_RELOCATABLE_INSTALL)
    set(DICOM_EDITOR_INSTALL_DATADIR "../${CMAKE_INSTALL_DATADIR}/dicom-dataset-editor")
    target_compile_definitions(dicom_editor_core
        PRIVATE DICOM_EDITOR_INSTALL_DATADIR="${DICOM_EDITOR_INSTALL_DATADIR}"
    )
    target_compile_definitions(dicom-dataset-editor
        PRIVATE DICOM_EDITOR_INSTALL_DATADIR="${DICOM_EDITOR_INSTALL_DATADIR}"
    )

    if(APPLE)
        set_property(TARGET dicom-dataset-editor PROPERTY INSTALL_RPATH
            "@loader_path/../${CMAKE_INSTALL_LIBDIR}"
        )
    elseif(UNIX)
        set_property(TARGET dicom-dataset-editor PROPERTY INSTALL_RPATH
            "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
        )
    endif()

    set_property(TARGET dicom-dataset-editor PROPERTY BUILD_RPATH_USE_ORIGIN TRUE)

    install(TARGETS dicom-dataset-editor
        RUNTIME_DEPENDENCY_SET dicom_editor_runtime_dependencies
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION .
    )

    if(APPLE)
        install(RUNTIME_DEPENDENCY_SET dicom_editor_runtime_dependencies
            POST_EXCLUDE_REGEXES
                "^/System/Library/"
                "^/usr/lib/"
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    elseif(UNIX)
        install(RUNTIME_DEPENDENCY_SET dicom_editor_runtime_dependencies
            POST_EXCLUDE_REGEXES
                "^/lib/"
                "^/lib64/"
                "^/usr/lib/"
                "^/usr/lib64/"
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    else()
        install(RUNTIME_DEPENDENCY_SET dicom_editor_runtime_dependencies
            PRE_EXCLUDE_REGEXES
                "^api-ms-"
                "^ext-ms-"
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    install(FILES
        "${dcmtk_PACKAGE_FOLDER_RELEASE}/bin/share/dcmtk-3.7.0/dicom.dic"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/dicom-dataset-editor/dcmtk"
    )
else()
    install(TARGETS dicom-dataset-editor
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION .
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(dicom_editor_tests tests/DicomEditorTests.cpp)
    target_link_libraries(dicom_editor_tests PRIVATE dicom_editor_core)
    add_test(NAME dicom_editor_tests COMMAND dicom_editor_tests)
endif()

if(DICOM_EDITOR_ENABLE_STRICT_WARNINGS)
    include(cmake/CompilerWarnings.cmake)
    dicom_editor_enable_strict_warnings(dicom_editor_core dicom-dataset-editor)
    if(BUILD_TESTING)
        dicom_editor_enable_strict_warnings(dicom_editor_tests)
    endif()
endif()

if(DICOM_EDITOR_DEVELOPER_MODE)
    include(cmake/DeveloperMode.cmake)
endif()
