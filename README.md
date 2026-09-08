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
- Edit Structured Report content in a separate tree and node form (`Edit > Structured Report...`, `Ctrl+R`).
- Toggle an aspect-fitted pixel data preview with separate file and frame navigation and a draggable split, either below or beside the
  dataset.
- Edit scalar values inline by double-clicking the `Value` column.
- Collapse sequences, individual items, or the dataset root using the `[+]`/`[-]`
  control, Enter, or Left/Right. `Collapse / Expand` above the dataset table
  clears the filter and toggles the selected branch (or its containing branch when
  an element is selected); with no selection it toggles the whole tree. Search
  includes collapsed descendants; clearing the filter restores the previous folds.
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

## Structured Reports

Open an SR file normally, then choose `Edit > Structured Report...` (`Ctrl+R`).
The report editor shows nested content items with their concept names
and value previews. Indentation shows parent-child structure; bracketed labels
show exact DICOM relationship names, including `CONTAINS`, `HAS PROPERTIES`,
and `INFERRED FROM`. Select a
node to see its technical value type and full relationship, and edit supported
value fields: text, codes, numeric measurements and units, dates, times, person names, and UID
references. Concept-name fields are hidden by default; enable `Edit concept
name (advanced)` when needed. The detail header shows the node relationship.
The node name is always shown above a separate Value section. Resize the
window to give the overview and value editor more room.
Drag the divider between the SR tree and details to adjust their widths.
The chosen proportion is retained when resizing the window. `Collapse / Expand`
above the SR tree toggles the selected branch or its containing branch; with no
selection, it toggles the complete report tree.

`Add copy` appends a copy of the selected node and all its descendants under
the same parent, then selects the copy for editing. Review its values before
saving. `Delete subtree...` removes the selected node and descendants after
confirmation. The report root cannot be copied or deleted. Tree changes are
blocked when the report contains by-reference content items.

Right-click a node for `Insert before...`, `Insert after...`, or `Add child...`.
New nodes can be TEXT, NUM, CODE, CONTAINER, DATE, TIME, DATETIME, PNAME, or
UIDREF. Enter a concept name (code, coding scheme, meaning), choose its
relationship to the parent, and fill in the type-specific value fields.
NUM requires a number and coded units; CODE requires a coded value. The form
keeps entered values when validation fails. The new node is selected after
creation. The root accepts children but cannot have siblings. Parent/type/
relationship constraints are checked for the report's SOP class.

`Apply node` validates and updates the in-memory document. `Close` returns to
the raw dataset editor; use the normal Save or Save All commands to persist
changes. Switching nodes or closing prompts about unapplied form changes.

Moving nodes or changing existing node types is not supported. Spatial/temporal coordinates,
image/composite/waveform references
remain in the dataset unchanged and can be inspected in the raw editor.
Numeric values remain editable when optional floating-point or rational
encodings exist. Changing the number removes those alternate encodings so
they cannot retain a conflicting value; unchanged numbers preserve them.
Verified or digitally signed reports are read-only in the SR editor. Validation
uses DCMTK value checks and dcmsr coded-entry checks; it does not certify report
templates or clinical content. Unrelated attributes and sequences are preserved.

## Quick Build

Register and authenticate the `myartifactory` Conan remote, then initialize the
project-local Conan home and install its locked configuration package as
documented in [BUILDING.md](BUILDING.md). Build the final optimized executable
with the Release Conan profile and the `production` CMake preset.

Linux:

```bash
conan install . --build=never --lockfile=conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release -c tools.build:skip_test=True
cmake --preset production
cmake --build --preset production
```

Windows:

```powershell
conan install . --build=never --lockfile=conan.lock -pr:h=windows-msvc-release -pr:b=windows-msvc-release -c tools.build:skip_test=True
cmake --preset production
cmake --build --preset production
```

`production` uses CMake's standard Release configuration. Daily development
uses a separate Debug profile and the `dev` preset; see [HACKING.md](HACKING.md).
Use [BUILDING.md](BUILDING.md) for installation and Conan package creation.
