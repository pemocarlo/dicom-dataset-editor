# Hacking

Developer mode is opt-in. It enables strict warnings, `clang-format`, `clang-tidy`, `cppcheck`, IWYU, tests, and `compile_commands.json`.

## Developer Tools

Conan installs pinned CMake and cppcheck versions as build requirements:

- `cmake/4.3.2`
- `cppcheck/2.20.0`

Both pins have ConanCenter binaries for Linux and Windows.

Run the normal Conan install before developer workflows:

```bash
conan install . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release
```

The checked-in developer presets currently target the Linux single-config
workflow. On Windows, use the Release workflow in [BUILDING.md](BUILDING.md).
Running the developer checks on Windows additionally requires a Ninja-based
profile so CMake can generate `compile_commands.json`, plus `clang-format` and
`clang-tidy` on `PATH`.

The Conan toolchain adds cppcheck to CMake's program search path, so the
`cppcheck` target does not depend on a system cppcheck. Conan also generates
build-environment activation scripts under `build/Release/generators` for pinned
command-line tools:

```bash
# Linux
source build/Release/generators/conanbuild.sh

# Windows cmd.exe
call build\Release\generators\conanbuild.bat
```

ConanCenter does not currently package `clang-format`, `clang-tidy`, `clangd`, or
`include-what-you-use`. Install one matching LLVM toolchain for these. IWYU must
match the LLVM major version it was built against. Tested developer-tool pairing:

- LLVM/Clang `22.1.6`
- include-what-you-use `0.26`, built against Clang `22.1.6`

On Windows, LLVM's installer provides `clang-format`, `clang-tidy`, and `clangd`.
IWYU still needs a separate compatible build or installation.

## Developer Workflow

```bash
cmake --workflow --preset dev-check
```

For IWYU too:

```bash
cmake --workflow --preset all-checks
```

Individual targets:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
cmake --build --preset format
cmake --build --preset check-format
cmake --build --preset lint
cmake --build --preset cppcheck
cmake --preset iwyu
cmake --build --preset iwyu
```

## Strict Warnings

Developer mode enables strict warnings by default. Override with
`-DDICOM_EDITOR_ENABLE_STRICT_WARNINGS=OFF` only when diagnosing compiler issues.

GCC flags include `-Wall`, `-Wextra`, `-Wpedantic`, `-Werror`, conversion checks, shadowing, format checks, switch coverage, null-dereference, pointer-arithmetic, overflow, fallthrough, VLA, and string literal diagnostics.

Why not every GCC warning:

- GCC has no real `-Weverything`.
- Some warnings are noisy, mutually incompatible, or target code outside this project.
- Some diagnostics are useful only when the compiler sees a very specific API shape.

## Clang-Tidy

Enabled groups focus on defects with high signal:

- `clang-analyzer-*`
- `bugprone-*`
- `performance-*`
- `portability-*`
- selected `readability-*`
- `modernize-use-nullptr`

Explicit exclusions:

- `bugprone-easily-swappable-parameters`: many GUI APIs intentionally pass same-type values.
- `bugprone-exception-escape`: GUI callback boundaries are not modeled well enough for this app.
- `bugprone-suspicious-call-argument`: heuristic name matching creates false positives.
- `bugprone-unsafe-functions`: broad policy check is too coarse for portable third-party APIs.
- `portability-avoid-pragma-once`: this project accepts `#pragma once`.

`WarningsAsErrors: '*'` stays enabled.

## Cppcheck

The `cppcheck` target analyzes the developer preset's `compile_commands.json` with warning, style, performance, and portability checks. Findings fail the target.

## IWYU

`include-what-you-use` runs through CMake's native `CXX_INCLUDE_WHAT_YOU_USE` support in the `iwyu` preset.

If the executable is local only, pass its path explicitly:

```bash
cmake --preset iwyu -DDICOM_EDITOR_IWYU_EXECUTABLE=/path/to/include-what-you-use
```

## Editor Support

Developer preset sets native CMake variable `CMAKE_EXPORT_COMPILE_COMMANDS=ON`.
Developer mode exposes generated `compile_commands.json` at source root for clangd and Neovim LSP.

Launch an editor from the activated Conan build environment when it needs the
Conan-managed CMake or cppcheck:

```bash
# Linux
source build/Release/generators/conanbuild.sh
code .
nvim .
emacs .

# Windows cmd.exe
call build\Release\generators\conanbuild.bat
code .
```

VS Code uses the checked-in `.vscode/settings.json`: CMake Tools uses the `dev`
preset and clangd reads source-root `compile_commands.json`. Launching VS Code
from the activated environment makes CMake Tools resolve Conan's CMake.

LazyVim's clangd setup discovers source-root `compile_commands.json`
automatically. If overriding its command, use `cmd = { "clangd",
"--compile-commands-dir=." }`; launch Neovim from the environment that contains
the intended clangd.

For Emacs, start it from the same environment and use clangd with Eglot:

```elisp
(add-to-list 'eglot-server-programs '((c++-mode c-mode) . ("clangd" "--compile-commands-dir=.")))
```

On Linux and macOS, `mise` can pin LLVM and launch editors without manually
activating a shell environment. Its current `clang` plugin builds LLVM from
source, so installation is slow. Configure LLVM `22.1.6` in your user or project
mise config:

```bash
mise use clang@22.1.6
mise exec -- code .
mise exec -- nvim .
mise exec -- emacs .
```

Keep Conan responsible for build dependencies and cppcheck; use mise only for
editor-facing LLVM tools that ConanCenter does not provide. On Windows, prefer
the official LLVM installer and point the editor at its `bin` directory.

## Code Layout

- `DicomDocument`: DICOM file ownership, load/save, dirty state, recursive node listing, and patient/study/series metadata.
- `EditorController`: multi-document workspace ownership, recursive folder loading, active-file workflows, and action state.
- `FileTreePanel`: collapsible patient/study/series/file workspace navigation.
- `PixelDataPanel`: optional scaled pixel preview with separate file/frame navigation and a resizable split view below or beside the dataset.
- `DicomPath`: stable path through sequence items and optional element tag.
- `DicomEditorService`: add/edit/delete operations.
- `DatasetViewModel`: filtering and row presentation.
- `EditorWindow`: FLTK menus, dialogs, and app-level event wiring.
- `DatasetPanel`: FLTK dataset table, focus, and keyboard navigation.
- `AttributeDialog`: FLTK tag/value entry.
