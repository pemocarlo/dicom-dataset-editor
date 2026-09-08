#pragma once

#include "dicom_editor/core/StructuredReport.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class Fl_Window;

using ReportEditHandler = std::function<bool(const dicom_editor::DicomPath &, const std::vector<std::string> &)>;
using ReportStructureHandler = std::function<bool(const dicom_editor::DicomPath &, bool, dicom_editor::DicomPath &)>;
using ReportReloadHandler = std::function<std::vector<dicom_editor::ReportNode>()>;
using ReportInsertHandler = std::function<bool(const dicom_editor::DicomPath &, dicom_editor::ReportInsertion,
                                               const dicom_editor::ReportNodeInput &, dicom_editor::DicomPath &)>;
using ReportCreateHandler = std::function<bool(const dicom_editor::ReportNodeInput &)>;

[[nodiscard]] std::unique_ptr<Fl_Window> createReportNodeDialog(ReportCreateHandler create);

/// Creates the content-tree form; its owner controls the modal event loop.
[[nodiscard]] std::unique_ptr<Fl_Window> createStructuredReportDialog(std::vector<dicom_editor::ReportNode> nodes, ReportEditHandler edit,
                                                                      ReportStructureHandler structure = {},
                                                                      ReportReloadHandler reload = {}, ReportInsertHandler insert = {});

/// Opens a modal content-tree editor for the active report.
void showStructuredReportDialog(std::vector<dicom_editor::ReportNode> nodes, ReportEditHandler edit, ReportStructureHandler structure = {},
                                ReportReloadHandler reload = {}, ReportInsertHandler insert = {});
