# Hacking

Developer mode is opt-in. It enables strict warnings, `clang-format`, `clang-tidy`, `cppcheck`, IWYU, tests, and `compile_commands.json`.

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

## Code Layout

- `DicomDocument`: DICOM file ownership, load/save, dirty state, recursive node listing.
- `DicomPath`: stable path through sequence items and optional element tag.
- `DicomEditorService`: add/edit/delete operations.
- `EditorController`: document workflows and action state.
- `DatasetViewModel`: filtering and row presentation.
- `EditorWindow`: FLTK menus, dialogs, and app-level event wiring.
- `DatasetPanel`: FLTK dataset table, focus, and keyboard navigation.
- `AttributeDialog`: FLTK tag/value entry.
