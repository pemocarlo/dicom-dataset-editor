# Contributing

## Before You Change Code

- Run the documented Conan install with the profile set required by your platform. The checked-in examples use the Linux `linux-gcc-release` profiles from `dicom-dataset-editor-conf`.
- Use `cmake --workflow --preset dev-check` before sending a change. It includes formatting, clang-tidy, cppcheck, and tests.
- Use `cmake --workflow --preset all-checks` if you touched include sets or headers.

## Style

- Keep edits ASCII unless the file already uses Unicode.
- Prefer `apply_patch` for manual file edits.
- Keep docs concise. Put developer detail in [HACKING.md](HACKING.md), build detail in [BUILDING.md](BUILDING.md), and overview in [README.md](README.md).

## Verification

- `cmake --build --preset conan-release`
- `ctest --preset conan-release --output-on-failure`
- `cmake --workflow --preset dev-check`
- `cmake --workflow --preset all-checks`

## Pull Requests

- Keep changes focused.
- Mention if you changed install behavior, developer tooling, or Conan profiles.
- If you add new headers or dependencies, run IWYU and update mappings if needed.
