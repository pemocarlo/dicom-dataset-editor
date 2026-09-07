#pragma once

#include "dicom_editor/core/StructuredReport.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class Fl_Window;

using ReportEditHandler = std::function<bool(const dicom_editor::DicomPath &, const std::vector<std::string> &)>;

/// Creates the content-tree form; its owner controls the modal event loop.
[[nodiscard]] std::unique_ptr<Fl_Window> createStructuredReportDialog(std::vector<dicom_editor::ReportNode> nodes, ReportEditHandler edit);

/// Opens a modal content-tree editor for the active report.
void showStructuredReportDialog(std::vector<dicom_editor::ReportNode> nodes, ReportEditHandler edit);
