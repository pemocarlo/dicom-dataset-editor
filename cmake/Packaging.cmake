include(GNUInstallDirs)

if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

# Install the executable into a conventional layout.  On Windows, also collect
# the non-system DLLs that the executable needs so the installed application can
# run without Conan or another development environment on PATH.
if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)

    # The Windows SDK lists API-set forwarding DLLs as part of the UCRT
    # redistributable, but those files are provided by Windows itself and must
    # not be copied into the application directory.
    list(FILTER CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS EXCLUDE REGEX
        "[\\\\/]((api|ext)-ms-).*"
    )
    install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )

    set(DICOM_EDITOR_RUNTIME_DEPENDENCY_DIRECTORIES
        "${CMAKE_BINARY_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
    )
    if(DEFINED CONAN_RUNTIME_LIB_DIRS)
        list(APPEND DICOM_EDITOR_RUNTIME_DEPENDENCY_DIRECTORIES
            ${CONAN_RUNTIME_LIB_DIRS}
        )
    endif()

    set(DICOM_EDITOR_RUNTIME_DEPENDENCIES_SET_ARG
        RUNTIME_DEPENDENCY_SET dicom_editor_runtime_dependencies
    )
endif()

install(TARGETS dicom-dataset-editor
    ${DICOM_EDITOR_RUNTIME_DEPENDENCIES_SET_ARG}
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

if(WIN32)
    install(RUNTIME_DEPENDENCY_SET dicom_editor_runtime_dependencies
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        DIRECTORIES ${DICOM_EDITOR_RUNTIME_DEPENDENCY_DIRECTORIES}
        PRE_EXCLUDE_REGEXES
            "api-ms-.*"
            "ext-ms-.*"
        POST_EXCLUDE_REGEXES
            ".*[\\/]([Ww][Ii][Nn][Dd][Oo][Ww][Ss]|system32|SysWOW64)[\\/].*"
    )
endif()

# ZIP is always available as a portable fallback.  Pass -G NSIS to CPack on a
# Windows build machine with NSIS installed to create the user-facing installer.
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "dicom-dataset-editor")
set(CPACK_PACKAGE_VENDOR "Dicom Dataset Editor")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "FLTK GUI for editing DICOM datasets")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "DicomDatasetEditor")
set(CPACK_VERBATIM_VARIABLES TRUE)

if(WIN32)
    set(CPACK_PACKAGE_FILE_NAME
        "dicom-dataset-editor-${PROJECT_VERSION}-windows-x64"
    )
    set(CPACK_NSIS_DISPLAY_NAME "Dicom Dataset Editor")
    set(CPACK_NSIS_PACKAGE_NAME "Dicom Dataset Editor")
    set(CPACK_NSIS_CONTACT "Dicom Dataset Editor")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MENU_LINKS
        "${CMAKE_INSTALL_BINDIR}/dicom-dataset-editor.exe;Dicom Dataset Editor"
    )
endif()

include(CPack)
