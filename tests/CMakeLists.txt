add_executable(dicom_editor_tests)
target_sources(dicom_editor_tests PRIVATE DicomEditorTests.cpp)
target_link_libraries(dicom_editor_tests
    PRIVATE
        Catch2::Catch2WithMain
        DicomDatasetEditor::application
)

add_executable(dicom_editor_gui_smoke_test)
target_sources(dicom_editor_gui_smoke_test PRIVATE GuiSmokeTest.cpp)
target_include_directories(dicom_editor_gui_smoke_test PRIVATE "${PROJECT_SOURCE_DIR}/src/gui/fltk")
target_link_libraries(dicom_editor_gui_smoke_test
    PRIVATE
        Catch2::Catch2WithMain
        DicomDatasetEditor::fltk
)

include(Catch)
catch_discover_tests(dicom_editor_tests
    DISCOVERY_MODE PRE_TEST
    TEST_PREFIX "unit."
    PROPERTIES LABELS unit
)
catch_discover_tests(dicom_editor_gui_smoke_test
    DISCOVERY_MODE PRE_TEST
    TEST_PREFIX "gui."
    PROPERTIES LABELS "gui;smoke"
)
