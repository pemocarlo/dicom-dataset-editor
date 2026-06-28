# AGENTS.md

Read this first if you are an agent working in this repo.

## Canonical Docs

- [README.md](README.md) for project overview.
- [BUILDING.md](BUILDING.md) for build and install flow.
- [HACKING.md](HACKING.md) for developer workflow and tooling.
- [CONTRIBUTING.md](CONTRIBUTING.md) for contribution rules.
- [ARCHITECTURE.md](ARCHITECTURE.md) for layers, ownership, and dependency rules.

## Rules

- Always use `rtk` for shell commands.
- Use `linux-gcc-debug` as host and `linux-gcc-release` as build profile for daily development.
- Use `linux-gcc-debug-ninja` as host for Ninja, quality, and IWYU presets.
- Use both `linux-gcc-release` profiles for the `production` preset and packaging.
- On Windows use equivalent `windows-msvc-*` profiles; keep Release as build context.
- Do not remove user changes outside task scope.
- Use `apply_patch` for manual edits.
- Prefer `rg` over `grep`, `rg --files` over `find` when practical.
- Keep changes ASCII unless file already uses Unicode.

## Verification

- `cmake --workflow --preset dev-check`
- `cmake --workflow --preset quality-checks` for extended static analysis
- `cmake --workflow --preset all-checks` when headers or include sets change
- `cmake --build --preset production` and `cmake --install build/Release --prefix "$HOME/tmp" --config Release` for relocatable install checks

## Current Architecture

- The application uses FLTK and DCMTK with C++23.
- Targets split into core domain, application controller, FLTK adapter, and thin executable composition root.
- `DicomWorkspace` owns documents, discovery/loading policy, DICOMDIR resolution, shared file ordering/navigation, dirty queries, reset, and
  batch scopes.
- `EditorController` orchestrates use cases through the abstract `EditorView`; keep FLTK logic out of core.
- `FileTreePanel` presents Patient/Study/Series/File hierarchy; `DatasetPanel` presents the active dataset.
- `PixelDataPanel` has independent open-file and multi-frame navigation.

## Useful Skills

- `caveman`: terse but accurate chat mode.
- `caveman-review`: fast code review output.
- `caveman-commit`: short conventional commit messages.
- `caveman-stats`: token usage visibility.
