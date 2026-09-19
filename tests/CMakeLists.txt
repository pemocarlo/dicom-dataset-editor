add_executable(dicom_editor_tests)
target_sources(dicom_editor_tests PRIVATE DicomEditorTests.cpp)
target_link_libraries(dicom_editor_tests
    PRIVATE
        Catch2::Catch2WithMain
        DicomDatasetEditor::application
)

if(DICOM_EDITOR_BUILD_FLTK)
    add_executable(dicom_editor_gui_smoke_test)
    target_sources(dicom_editor_gui_smoke_test PRIVATE GuiSmokeTest.cpp)
    target_include_directories(dicom_editor_gui_smoke_test PRIVATE "${PROJECT_SOURCE_DIR}/src/gui/fltk")
    target_link_libraries(dicom_editor_gui_smoke_test
        PRIVATE
            Catch2::Catch2WithMain
            DicomDatasetEditor::fltk
    )
endif()

include(Catch)
catch_discover_tests(dicom_editor_tests
    DISCOVERY_MODE PRE_TEST
    TEST_PREFIX "unit."
    PROPERTIES LABELS unit
)

add_executable(dicom_viewer_operations_smoke_test)
target_sources(dicom_viewer_operations_smoke_test PRIVATE DicomViewerOperationsSmoke.cpp)
target_link_libraries(dicom_viewer_operations_smoke_test
    PRIVATE
        Catch2::Catch2WithMain
        DicomViewer::operations
)

catch_discover_tests(dicom_viewer_operations_smoke_test
    DISCOVERY_MODE PRE_TEST
    TEST_PREFIX "unit.operations."
    PROPERTIES LABELS unit
)

if(DICOM_EDITOR_BUILD_FLTK)
    catch_discover_tests(dicom_editor_gui_smoke_test
        DISCOVERY_MODE PRE_TEST
        TEST_PREFIX "gui."
        PROPERTIES LABELS "gui;smoke"
    )
endif()
