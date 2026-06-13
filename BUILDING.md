# Building

This project uses Conan profiles from the installed `dicom-dataset-editor-conf` package. On Linux, build with the profile set from that package:

```bash
conan install . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release
```

That command generates `build/Release/generators/CMakePresets.json` and the Conan toolchain.
It also passes the DCMTK dictionary location required by the application to CMake.

In-source builds are not supported.

## Build And Test

```bash
cmake --preset conan-release
cmake --build --preset conan-release
ctest --preset conan-release
```

Run the Conan install again after changing the recipe, lockfile, profiles, or dependencies.
CMake configuration is expected to use the generated Conan toolchain; configuring without it requires manually providing all dependencies and `DICOM_EDITOR_DCMTK_DICT_FILE`.

## Install

Install to any prefix with:

```bash
cmake --install build/Release --prefix <your-install-prefix>
```

Expected layout:

- `<your-install-prefix>/bin/dicom-dataset-editor`
- `<your-install-prefix>/share/dicom-dataset-editor/dcmtk/dicom.dic`

The executable locates installed runtime data relative to itself, so the complete install prefix can be moved. CMake does not bundle dependency libraries; provide them through the system, Conan, or a platform-specific deployment step.

## Conan Package

Build, test, and package the application with:

```bash
conan create . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release -c tools.build:skip_test=False
```

Use `-c tools.build:skip_test=True` when package creation should skip tests.
