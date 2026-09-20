cmake_minimum_required(VERSION 4.0)

project(DicomViewerPackageTest LANGUAGES CXX)

find_package(DicomViewer CONFIG REQUIRED)

add_executable(dicom_viewer_package_test src/main.cpp)
target_link_libraries(dicom_viewer_package_test PRIVATE DicomViewer::operations)
