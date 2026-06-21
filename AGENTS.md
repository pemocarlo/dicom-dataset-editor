# AGENTS.md

Read this first if you are an agent working in this repo.

## Canonical Docs

- [README.md](README.md) for project overview.
- [BUILDING.md](BUILDING.md) for build and install flow.
- [HACKING.md](HACKING.md) for developer workflow and tooling.
- [CONTRIBUTING.md](CONTRIBUTING.md) for contribution rules.

## Rules

- Always use `rtk` for shell commands.
- Always build with `conan install . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release`.
- Do not remove user changes outside task scope.
- Use `apply_patch` for manual edits.
- Prefer `rg` over `grep`, `rg --files` over `find` when practical.
- Keep changes ASCII unless file already uses Unicode.

## Verification

- `cmake --workflow --preset dev-check`
- `cmake --workflow --preset all-checks` when headers or include sets change
- `cmake --install build/Release --prefix "$HOME/tmp"` for relocatable install checks

## Current Architecture

- The application uses FLTK and DCMTK with C++23.
- `DicomWorkspace` owns documents, discovery/loading policy, DICOMDIR reference resolution, file sorting, batch scopes, and active-file state.
- `EditorController` orchestrates use cases through the abstract `EditorView`; keep FLTK logic out of core.
- `FileTreePanel` presents Patient/Study/Series/File hierarchy; `DatasetPanel` presents the active dataset.
- `PixelDataPanel` has independent open-file and multi-frame navigation.

## Useful Skills

- `caveman`: terse but accurate chat mode.
- `caveman-review`: fast code review output.
- `caveman-commit`: short conventional commit messages.
- `caveman-stats`: token usage visibility.
