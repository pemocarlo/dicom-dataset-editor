# DICOM Dataset Editor

C++23 FLTK GUI for opening, inspecting, editing, and saving DICOM datasets through DCMTK.

## Start Here

- Build and install: [BUILDING.md](BUILDING.md)
- Developer workflow and tooling: [HACKING.md](HACKING.md)
- Contribution flow: [CONTRIBUTING.md](CONTRIBUTING.md)
- Architecture and code boundaries: [ARCHITECTURE.md](ARCHITECTURE.md)

## What It Does

- Open one or many DICOM files at once.
- Recursively scan a folder and keep every valid DICOM file open.
- Open DICOMDIR media directories by following their referenced file records.
- Browse open files in a patient/study/series hierarchy, sorted by Instance Number by default, and switch files without losing edits.
- Batch-edit patient- or study-level attributes after reviewing consistency across matching datasets.
- Browse recursive dataset tree, including sequence items.
- Toggle an aspect-fitted pixel data preview with separate file and frame navigation and a draggable split, either below or beside the
  dataset.
- Edit scalar values inline by double-clicking the `Value` column.
- Optionally validate DICOM values and highlight invalid values in red.
- Add, delete, save, and reload datasets.
- Carry DCMTK data dictionary inside executable and optionally load updated
  DCMTK-format dictionary for current session.

## File Navigation

- `File > Open Files...` (`Ctrl+O`) accepts multiple `*.dcm` selections. Folder and DICOMDIR choosers remain unfiltered.
- `File > Open Folder...` (`Ctrl+Shift+O`) scans the selected folder and its subfolders. Files that DCMTK cannot parse are skipped and
  summarized after the scan.
- `File > Open DICOMDIR...` opens only files referenced by the selected DICOMDIR. Selecting a DICOMDIR as an ordinary dataset does not add
  the directory object to the workspace.
- Select a leaf in the left sidebar to activate that dataset. The hierarchy uses Patient, Study, and Series DICOM attributes, with stable
  identifiers shown where available.
- File leaves show filenames only. Right-click a file for its full path and hierarchy details. Right-click a patient or study to review
  consistency and batch-edit supported attributes across that group.
- Files are ordered numerically by DICOM Instance Number, with missing numbers last. Toggle `View > Sort Files by Filename` for lexical
  filename order. Previous/next controls in both main view and pixel preview follow this same visible order.
- The open-files panel stays hidden until a dataset is loaded. Toggle it with `View > Open Files Panel` and drag its right edge to resize it.
- `View > Previous File` (`Ctrl+Page Up`) and `View > Next File` (`Ctrl+Page Down`) navigate the open workspace. The same controls are
  available in the pixel preview beside the independent frame controls.
- `View > Show Pixel Preview` (`Ctrl+P`) opens an adaptively sized preview pane. Use its controls for fit-to-pane, 1:1, and incremental
  zoom; use the mouse wheel to zoom around the pointer, drag to pan, or double-click to fit. Placement commands move the pane right or
  below without ambiguous checked layout options, and each placement remembers its own size.
- Bold text and an asterisk mark an open file with unsaved changes. Switching files and batch edits keep changes in memory; save affected
  datasets normally, or use `File > Save All` (`Ctrl+Alt+S`). Batch value entry starts with existing value for small corrections.
- `File > Clear Workspace` (`Ctrl+W`) resolves unsaved changes then returns to one empty dataset. Closing with multiple dirty datasets
  offers one `Discard All`, `Save All`, or `Cancel` choice instead of prompting once per file.

## Quick Build

Initialize the project-local Conan home and install its configuration package as
documented in [BUILDING.md](BUILDING.md). Build the final optimized executable
with the Release Conan profile and the `production` CMake preset.

Linux:

```bash
conan install . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release -c tools.build:skip_test=True
cmake --preset production
cmake --build --preset production
```

Windows (prefer an **x64 Native Tools Command Prompt for Visual Studio**, which
sets up MSVC and the Windows SDK):

```batch
conan install . --build=missing --lockfile=conan.lock -pr:h=windows-msvc-release -pr:b=windows-msvc-release -c tools.build:skip_test=True
call build\Release\generators\conanbuild.bat
cmake --preset production
cmake --build --preset production
cmake --install build\Release --prefix build\install --config Release
build\install\bin\dicom-dataset-editor.exe
```

`production` uses CMake's standard Release configuration. Daily development
uses a separate Debug profile and the `dev` preset; see [HACKING.md](HACKING.md).
Before the first command, bootstrap the project-local Conan configuration as
described in [BUILDING.md](BUILDING.md). Use that guide for the full Windows
setup, installation details, and Conan package creation.
