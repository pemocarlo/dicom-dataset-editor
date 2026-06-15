# DICOM Dataset Editor

C++23 FLTK GUI for opening, inspecting, editing, and saving DICOM datasets through DCMTK.

## Start Here

- Build and install: [BUILDING.md](BUILDING.md)
- Developer workflow and tooling: [HACKING.md](HACKING.md)
- Contribution flow: [CONTRIBUTING.md](CONTRIBUTING.md)

## What It Does

- Open DICOM files.
- Browse recursive dataset tree, including sequence items.
- Edit scalar values inline by double-clicking the `Value` column.
- Add, delete, save, and reload datasets.

## Quick Build

If you already have the Conan config package installed, use its profile for your platform.

Linux:

```bash
conan install . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release
cmake --preset conan-release
cmake --build --preset conan-release
ctest --preset conan-release
```

Windows:

```powershell
conan install . --build=missing --lockfile=conan.lock -pr:h=windows-msvc-release -pr:b=windows-msvc-release
conan build . -pr:h=windows-msvc-release -pr:b=windows-msvc-release
```

Use [BUILDING.md](BUILDING.md) for install and Conan package creation.
