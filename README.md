# DICOM Dataset Editor

## Scope

This project is a C++23 wxWidgets GUI for opening, inspecting, editing, and saving DICOM datasets through DCMTK.

## Toolchain

- GCC 16
- Conan 2.29+
- CMake 4.x
- `wxwidgets/3.3.2`
- `dcmtk/3.7.0`
- Conan `CMakeToolchain`
- Experimental Conan `CMakeConfigDeps`
- `clang-format`, `clang-tidy`, and include-what-you-use for developer checks

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
conan lock create . --lockfile=conan.lock --lockfile-out conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release
conan install . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release
cmake --preset conan-release
cmake --build --preset conan-release
ctest --preset conan-release
```

Always build this project with `-pr:h=linux-gcc-release -pr:b=linux-gcc-release`. The profiles come from the installed Conan configuration package and contain the supported compiler, C++ standard, shared-library, and dependency-feature options.

`conan config install-pkg .` reads `conanconfig.yml` and installs the `dicom-dataset-editor-conf/0.1.0` configuration package. `--lockfile=""` avoids using a stale project lockfile before the config package is installed, and `--lockfile-out conan.lock` records the installed config package in `config_requires`. Export `../conan-conf` locally before installing the config package unless that package has already been uploaded to a configured Conan remote.

## Developer Mode

Developer mode enables tests, strict compiler warnings, formatting and linting targets, and a compilation database for editor tooling:

```bash
cmake --workflow --preset dev-check
```

The workflow configures and builds the developer tree with strict warnings, checks formatting, runs `clang-tidy`, and runs tests. Individual steps remain available:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
cmake --build --preset check-format
cmake --build --preset lint
```

Apply `clang-format` changes with:

```bash
cmake --build --preset format
```

The developer preset enables `DICOM_EDITOR_ENABLE_STRICT_WARNINGS`. GCC builds use a broad, practical warning set including `-Wall`, `-Wextra`, `-Wpedantic`, conversion, shadowing, format, virtual-dispatch, switch, and null-dereference diagnostics. `-Werror` makes every enabled compiler warning fail the build. The project does not blindly enable every GCC warning flag: GCC has no `-Weverything`, and many optional warnings are language-specific, mutually incompatible, intentionally noisy, or diagnose external APIs rather than project defects.

`clang-tidy` also treats every enabled diagnostic as an error. Its checks are deliberately selected rather than enabling `*`:

- Platform and vendor suites such as Android, Fuchsia, LLVM, and Google enforce unrelated environments or style guides.
- Broad policy suites such as `cppcoreguidelines-*` and `hicpp-*` contain overlapping checks and rules that conflict with wxWidgets/DCMTK APIs.
- Subjective metrics and style checks such as identifier length, magic numbers, and function size create noise without proving defects.
- `bugprone-easily-swappable-parameters` flags many intentional same-type GUI parameters.
- `bugprone-exception-escape` cannot model wxWidgets callback and application exception boundaries reliably.
- `bugprone-suspicious-call-argument` relies on parameter-name similarity and produces heuristic false positives.
- `bugprone-unsafe-functions` applies a broad API policy unsuitable for portable third-party GUI and DICOM APIs.
- `portability-avoid-pragma-once` conflicts with this project's accepted header style.

Enabled clang-tidy groups focus on analyzer defects, bug-prone constructs, performance, portability, duplicate/redundant code, and selected modernization. Keep exclusions explicit in `.clang-tidy`; add a check only when its findings can be kept warning-free.

Include-what-you-use uses CMake's native `CXX_INCLUDE_WHAT_YOU_USE` integration in a dedicated clean build:

```bash
cmake --preset iwyu
cmake --build --preset iwyu
```

IWYU is experimental and tightly coupled to its Clang version. Install an IWYU release matching installed Clang and ensure `include-what-you-use` is in `PATH`. A system-wide installation is not required; for a local installation outside `PATH`, configure with:

```bash
cmake --preset iwyu -DDICOM_EDITOR_IWYU_EXECUTABLE="$HOME/.local/bin/include-what-you-use"
```

The dedicated build is clean because IWYU runs only when sources compile. Findings fail the build through `--error=1`. IWYU alone ignores unknown warning names because it parses the GCC command line with Clang; GCC still enforces every configured warning. `iwyu.imp` maps wxWidgets platform-private implementation headers back to portable public headers, preventing Linux-only `wx/gtk/*` suggestions.

To run strict compilation, formatting, clang-tidy, tests, and IWYU together:

```bash
cmake --workflow --preset all-checks
```

The `dev` preset inherits Conan's generated release preset but uses a separate `build/Dev` directory. Run the documented `conan install` command with both `linux-gcc-release` profiles first so `build/Release/generators/CMakePresets.json` and the Conan toolchain exist.

Developer mode sets `DICOM_EDITOR_ENABLE_LSP=ON`. This enables `CMAKE_EXPORT_COMPILE_COMMANDS` and creates the source-root `compile_commands.json` symlink used automatically by `clangd`, including Neovim LSP clients. To enable only the compilation database in another configuration, pass `-DDICOM_EDITOR_ENABLE_LSP=ON`.

Developer tooling is opt-in. Normal release configuration does not require `clang-format`, `clang-tidy`, or IWYU, and does not enable warnings as errors.

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
