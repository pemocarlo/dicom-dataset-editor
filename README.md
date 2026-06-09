# DICOM Dataset Editor

## Scope

This project is a C++20 wxWidgets GUI for opening, inspecting, editing, and saving DICOM datasets through DCMTK.

## Toolchain

- GCC 16
- Conan 2.29+
- CMake 4.x
- `wxwidgets/3.3.2`
- `dcmtk/3.7.0`
- Conan `CMakeToolchain`
- Experimental Conan `CMakeConfigDeps`

## Architecture

- `DicomDocument` owns the DCMTK `DcmFileFormat`, file path, dirty state, load/save operations, recursive node listing, and typed path resolution.
- `DicomPath` is a typed, stable path through sequence item parents plus an optional element tag.
- `DicomNode` is an immutable GUI-facing row model containing tag, keyword, VR, VM, path, and value preview.
- `DicomEditorService` performs fail-fast add, edit, and delete operations.
- `MainFrame` owns menus, status, save prompts, and save/reload verification.
- `DatasetTreePanel` displays a recursive table with filtering.
- `AttributeEditDialog` provides raw VR fallback editing and tag/value entry for additions.

## Build

```bash
conan export-pkg ../conan-conf
conan config install-pkg . --lockfile="" --lockfile-out conan.lock --force
conan lock create . --lockfile=conan.lock --lockfile-out conan.lock -pr:h=default -pr:b=default
conan install . --build=missing --lockfile=conan.lock -pr:h=default -pr:b=default
cmake --preset conan-release
cmake --build --preset conan-release
ctest --preset conan-release
```

`conan config install-pkg .` reads `conanconfig.yml` and installs the `dicom-dataset-editor-conf/0.1.0` configuration package. `--lockfile=""` avoids using a stale project lockfile before the config package is installed, and `--lockfile-out conan.lock` records the installed config package in `config_requires`. Export `../conan-conf` locally before installing the config package unless that package has already been uploaded to a configured Conan remote.

## Install

Installing is always explicit. To create a relocatable, per-user installation without writing system-wide files:

```bash
cmake --preset conan-release -DDICOM_EDITOR_RELOCATABLE_INSTALL=ON
cmake --build --preset conan-release
cmake --install build/Release --prefix "$HOME/.local"
```

This installs:

- `~/.local/bin/dicom-dataset-editor`
- Bundled non-system shared libraries under `~/.local/lib`
- Runtime data under `~/.local/share/dicom-dataset-editor`

On Linux and macOS, the installed executable uses a relative runtime path from `bin` to `lib`. Runtime data is also located relative to the executable, so the complete prefix can be moved. System libraries remain system-provided and are not copied. On Linux, the relocatable build uses GTK's built-in simple input context because matching system GTK input modules cannot safely load into the bundled GTK libraries. On Windows, required non-system DLLs are installed beside the executable in `bin`.

`DICOM_EDITOR_RELOCATABLE_INSTALL` defaults to `OFF`. Without it, `cmake --install` installs only the executable. Use `--prefix` with any writable destination, or set `CMAKE_INSTALL_PREFIX` while configuring. Prefer `--prefix` for one-off installs because it does not change the configured build tree.

## Implemented Features

- Open DICOM file.
- Display recursive dataset rows, including sequence items.
- Show tag, keyword, VR, VM, path, and value preview.
- Edit values inline by double-clicking a Value cell.
- Edit non-sequence attributes with DCMTK validation.
- Navigate and edit attributes inside sequence items.
- Add and delete normal attributes.
- Save and Save As.
- Reload after saving to verify persistence.

## Verification Targets

- Scalar edit.
- Add/delete element.
- Nested sequence edit.
- Save/reload persistence.
- Recursive node listing.
