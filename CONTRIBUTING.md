# Contributing

## Before You Change Code

- Install Debug host dependencies with the Release build profile for your platform: `linux-gcc-debug`/`linux-gcc-release` or `windows-msvc-debug`/`windows-msvc-release`.
- Use `cmake --workflow --preset dev-check` during daily development. It builds Debug with strict warnings, checks formatting, and runs tests.
- Install the platform's `*-debug-ninja` host profile before `dev-check-ninja`, `quality-checks`, or `all-checks`.
- Run `cmake --workflow --preset quality-checks` before sending changes that warrant the slower clang-tidy and cppcheck pass.
- Use `cmake --workflow --preset all-checks` if you touched headers or include sets.
- On Linux, run `cmake --workflow --preset sanitize-checks` for memory-safety or ownership changes.
- Run `cmake --workflow --preset thread-checks` for concurrency changes and `valgrind-checks` when independent memory analysis is useful.

## Style

- Keep edits ASCII unless the file already uses Unicode.
- Prefer `apply_patch` for manual file edits.
- Prefer C++23 language and library features when they make code clearer or
  more explicit, but do not add cleverness for its own sake.
- Keep docs concise. Put developer detail in [HACKING.md](HACKING.md), build detail in [BUILDING.md](BUILDING.md), and overview in [README.md](README.md).

## Verification

- `cmake --workflow --preset dev-check`
- `cmake --workflow --preset quality-checks`
- `cmake --workflow --preset all-checks`
- `cmake --workflow --preset sanitize-checks` on Linux for memory-safety changes
- `cmake --workflow --preset thread-checks` on Linux for concurrency changes
- `cmake --build --preset production` for final executable or install changes
- `conan create . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release -c tools.build:skip_test=False` when changing CMake, Conan, or install behavior

## Pull Requests

- Keep changes focused.
- Mention if you changed install behavior, developer tooling, or Conan profiles.
- If you add new headers or dependencies, run IWYU and update mappings if needed.
