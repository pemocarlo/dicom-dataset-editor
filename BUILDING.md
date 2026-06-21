# Building

This project uses Conan profiles from the installed `dicom-dataset-editor-conf` package.

## Install Or Update The Conan Configuration

No custom Conan remote exists yet. The configuration recipe and package are
handled locally in the Conan cache from the sibling repository.

Create the configuration package from the sibling configuration repository, upgrade
its revision in the lockfile, and only then install it:

```bash
conan create ../dicom-dataset-editor-conf -s os=Linux
conan lock upgrade-config . --no-remote --lockfile=conan.lock --lockfile-out=conan.lock --update-config-requires=dicom-dataset-editor-conf/0.2.0
conan config install-pkg conanconfig.yml --lockfile=conan.lock --force
```

Commit the updated `conan.lock` when its configuration package revision changes.

On Linux:

```bash
conan install . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release
```

On Windows from PowerShell:

```powershell
conan install . --build=missing --lockfile=conan.lock -pr:h=windows-msvc-release -pr:b=windows-msvc-release
```

That command generates `build/Release/generators/CMakePresets.json` and the Conan toolchain.
It also passes DCMTK dictionary location to CMake. Dictionary is embedded during
build; installed application does not depend on Conan cache or separate data file.
Pinned build tools declared by the recipe are installed in Conan's build context and
recorded by the lockfile. Activate the generated Conan build environment before
running CMake to select the pinned CMake. The Conan toolchain adds pinned build
tools to CMake's program search path.

In-source builds are not supported.

## Build And Test

On Linux, use the generated single-config CMake presets:

```bash
cmake --preset conan-release
cmake --build --preset conan-release
ctest --preset conan-release
```

On Windows, use Conan to drive the Visual Studio configure, build, and tests.
Conan names Visual Studio's generated configure preset `conan-default`, so it
does not match the Linux-oriented checked-in presets.

```powershell
conan build . -pr:h=windows-msvc-release -pr:b=windows-msvc-release
```

Run the Conan install again after changing the recipe, lockfile, profiles, or dependencies.
CMake configuration is expected to use the generated Conan toolchain; configuring without it requires manually providing all dependencies and `DICOM_EDITOR_DCMTK_DICT_FILE`.

## Install

Install to any prefix with:

```bash
cmake --install build/Release --prefix <your-install-prefix>
```

Visual Studio builds require the configuration:

```powershell
cmake --install build/Release --prefix build/install --config Release
.\build\install\bin\dicom-dataset-editor.exe
```

Expected layout:

- `<your-install-prefix>/bin/dicom-dataset-editor`
- `<your-install-prefix>/bin/dicom-dataset-editor.exe` on Windows

Dictionary is compiled into executable, so install has no runtime data directory.
CMake does not bundle dependency libraries; provide them through system, Conan,
or platform-specific deployment step.
The Windows executable uses the GUI subsystem and does not open a separate console window.

## Conan Package

Build, test, and package the application with:

```bash
conan create . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release -c tools.build:skip_test=False
```

Use `-c tools.build:skip_test=True` when package creation should skip tests.
