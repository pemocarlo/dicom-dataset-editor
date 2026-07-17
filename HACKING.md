# Hacking

Daily developer mode is a Debug build with assertions, tests, strict compiler
warnings, and `clang-format`. It uses the platform's default generator: Unix
Makefiles on Linux and Visual Studio on Windows. Ninja remains available for a
compilation database and the slower clang-tidy, cppcheck, and IWYU checks.

Conan commands use the ignored, project-local `conanhome` selected by `.conanrc`.
Bootstrap its profiles and seed its package cache as described in
[BUILDING.md](BUILDING.md) before running developer workflows offline.

## Developer Tools

Conan installs pinned CMake and cppcheck on Linux and Windows. Ninja profiles
add pinned Ninja:

- `cmake/4.3.2`
- `cppcheck/2.20.0`
- `ninja/1.13.2`

All three have x86-64 ConanCenter binaries for Linux and Windows.

Install Debug host dependencies before the default developer workflow. Build
tools stay in Release so Conan does not rebuild them as Debug packages:

```bash
# Linux
conan install . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-debug -pr:b=linux-gcc-release

# Windows x64 Native Tools Command Prompt
conan install . --build=missing --lockfile=conan.lock -pr:h=windows-msvc-debug -pr:b=windows-msvc-release
```

Install the separate Ninja toolchain before `dev-ninja`, `quality`, or `iwyu`:

```bash
# Linux
conan install . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-debug-ninja -pr:b=linux-gcc-release

# Windows x64 Native Tools Command Prompt
conan install . --build=missing --lockfile=conan.lock -pr:h=windows-msvc-debug-ninja -pr:b=windows-msvc-release
```

The checked-in presets separate build intent and tool cost:

- `dev` is the normal Debug edit-build-test configuration.
- `dev-ninja` is the same daily configuration using Ninja and a compilation database.
- `production` is the optimized Release configuration used for the final executable.
- `quality` adds clang-tidy and cppcheck in a separate Ninja Debug build tree.
- `iwyu` adds include-what-you-use to the extended checks in another clean build tree.

Conan adds CMake and cppcheck to the generated build environment; Ninja profiles
also add Ninja. Use the script matching the installed profile before running a
preset:

```bash
# Linux default generator
source build/Debug/generators/conanbuild.sh

# Linux Ninja
source build/Ninja-Debug/generators/conanbuild.sh

# Windows x64 Native Tools Command Prompt, default generator
call build\Debug\generators\conanbuild.bat

# Windows x64 Native Tools Command Prompt, Ninja
call build\Ninja-Debug\generators\conanbuild.bat
```

External LLVM tools are intentionally not Conan requirements. Required tools by
workflow:

- `dev-check`: compiler, CMake, and `clang-format`.
- `dev-check-ninja`: the above plus Ninja.
- `quality-checks`: the above plus `clang-tidy` and cppcheck.
- `all-checks`: the above plus `include-what-you-use`.

Use this tested LLVM/IWYU pairing on both platforms:

- LLVM/Clang `22.1.6`
- include-what-you-use `0.26`, built against Clang `22.1.6`

On Windows, install the official
[LLVM 22.1.6 x64 package](https://github.com/llvm/llvm-project/releases/tag/llvmorg-22.1.6)
and add its `bin` directory to `PATH`; it provides `clang-format`, `clang-tidy`,
and `clangd`. IWYU 0.26 publishes source only. For `all-checks`, build the
[`clang_22` branch](https://github.com/include-what-you-use/include-what-you-use/tree/clang_22)
against the full LLVM x64 archive and add the resulting
`include-what-you-use.exe` to `PATH`. The
[official IWYU instructions](https://github.com/include-what-you-use/include-what-you-use/blob/clang_22/README.md)
require `--driver-mode=cl` for MSVC; this project supplies that argument
automatically.

Example Windows IWYU build after extracting the full LLVM archive to the path
shown below:

```batch
call build\Ninja-Debug\generators\conanbuild.bat
set "LLVM_ROOT=C:\Tools\clang+llvm-22.1.6-x86_64-pc-windows-msvc"
git clone --branch clang_22 --depth 1 https://github.com/include-what-you-use/include-what-you-use.git iwyu-src
cmake -S iwyu-src -B iwyu-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%LLVM_ROOT%" -DCMAKE_INSTALL_PREFIX="%LLVM_ROOT%"
cmake --build iwyu-build
cmake --install iwyu-build
set "PATH=%LLVM_ROOT%\bin;%PATH%"
```

Run Windows commands from an x64 Native Tools Command Prompt so `cl.exe`, the
Windows SDK, and MSVC environment variables are available. Verify setup with:

```batch
cmake --version
ninja --version
clang-format --version
clang-tidy --version
cppcheck --version
include-what-you-use --version
```

## Developer Workflow

Fast daily verification builds Debug with strict warnings, checks formatting,
and runs tests:

```bash
cmake --workflow --preset dev-check
```

Use Ninja for the same daily workflow when a compilation database is wanted:

```bash
cmake --workflow --preset dev-check-ninja
```

Run slower clang-tidy and cppcheck analysis when preparing substantial changes:

```bash
cmake --workflow --preset quality-checks
```

Run every check, including a clean IWYU build, after changing headers or include
sets:

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
cmake --preset dev-ninja
cmake --build --preset dev-ninja
ctest --preset dev-ninja
cmake --preset quality
cmake --build --preset quality
cmake --build --preset lint
cmake --build --preset cppcheck
cmake --preset iwyu
cmake --build --preset iwyu
```

## Performance Benchmark

An opt-in synthetic benchmark measures workspace opening, repeated file-tree
projection, and active dataset projection without requiring patient data. It
generates temporary DICOM files with 256 KiB Pixel Data and nested metadata,
then removes them after the run.

```bash
cmake --preset dev -DDICOM_EDITOR_BUILD_BENCHMARKS=ON
cmake --build --preset dev --target dicom_editor_benchmarks
./build/Debug/dicom_editor_benchmarks 100
```

The optional argument is the number of generated files. Run benchmarks in a
stable Release build when comparing changes; Debug results are useful only for
quick local checks. Benchmarks are not registered with CTest and do not affect
normal builds.

## Strict Warnings

All developer configurations enable strict warnings by default. Override with
`-DDICOM_EDITOR_ENABLE_STRICT_WARNINGS=OFF` only when diagnosing compiler issues.

GCC flags include `-Wall`, `-Wextra`, `-Wpedantic`, `-Werror`, conversion checks, shadowing, format checks, switch coverage, null-dereference, pointer-arithmetic, overflow, fallthrough, VLA, and string literal diagnostics.

Why not every GCC warning:

- GCC has no real `-Weverything`.
- Some warnings are noisy, mutually incompatible, or target code outside this project.
- Some diagnostics are useful only when the compiler sees a very specific API shape.

## Clang-Tidy

The `quality` and `iwyu` presets provide the `lint` target. Enabled groups focus
on defects with high signal:

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

The `quality` and `iwyu` presets provide the `cppcheck` target. It analyzes that
build tree's `compile_commands.json` with warning, style, performance, and
portability checks. Findings fail the target.

## IWYU

`include-what-you-use` runs through CMake's native `CXX_INCLUDE_WHAT_YOU_USE` support in the `iwyu` preset.

If the executable is local only, pass its path explicitly:

```bash
cmake --preset iwyu -DDICOM_EDITOR_IWYU_EXECUTABLE=/path/to/include-what-you-use
```

## Editor Support

Ninja developer presets set native CMake variable
`CMAKE_EXPORT_COMPILE_COMMANDS=ON` and expose `compile_commands.json` at source
root. The default Visual Studio workflow uses Visual Studio's native project
model and does not require a compilation database.

Launch an editor from the activated Conan build environment when it needs the
Conan-managed CMake or cppcheck:

```bash
# Linux Ninja
source build/Ninja-Debug/generators/conanbuild.sh
code .
nvim .
emacs .

# Windows x64 Native Tools Command Prompt, Ninja
call build\Ninja-Debug\generators\conanbuild.bat
code .
```

VS Code uses the checked-in `.vscode/settings.json`: CMake Tools uses the `dev`
preset and works with either Unix Makefiles or Visual Studio. Select
`dev-ninja` when clangd needs source-root `compile_commands.json`. Launching VS
Code from the matching activated environment makes CMake Tools resolve Conan's
CMake and generator.

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

- See [ARCHITECTURE.md](ARCHITECTURE.md) for dependency boundaries, state
  ownership, request flow, and extension rules.
- `DicomDocument`: DICOM file ownership, load/save, dirty state, recursive node listing, and patient/study/series metadata.
- `DicomWorkspace`: document ownership, recursive discovery, DICOMDIR reference resolution, shared sorting/navigation, dirty-state queries,
  reset, scoped batch edits, and duplicate handling.
- `DicomDictionary`: embedded DCMTK dictionary bootstrap and validated session override.
- `EditorController`: application-layer use-case orchestration through abstract `EditorView`.
- `FileTreePanel`: collapsible workspace presentation, context menus, and typed activation/batch-edit event forwarding.
- `PixelDataPanel`: optional scaled pixel preview with separate file/frame navigation and a resizable split view below or beside the dataset.
- `DicomPath`: stable path through sequence items and optional element tag.
- `DicomEditorService`: validated add/edit/delete/upsert operations.
- `DatasetViewModel`: filtering and row presentation.
- `EditorWindow`: FLTK adapter for menus, dialogs, layout, and app-level event wiring.
- `DatasetPanel`: FLTK dataset table, focus, and keyboard navigation.
- `AttributeDialog`: FLTK tag/value entry.
