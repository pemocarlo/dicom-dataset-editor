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

if(DICOM_EDITOR_ENABLE_LSP)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

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
    install(FILES resources/fonts.conf
        DESTINATION "${CMAKE_INSTALL_DATADIR}/dicom-dataset-editor/fontconfig"
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

if(DICOM_EDITOR_DEVELOPER_MODE)
    include(cmake/DeveloperMode.cmake)
endif()
