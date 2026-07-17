# Building

This project uses Conan profiles from the installed `dicom-dataset-editor-conf` package.

## Project Conan Home

The checked-in [`.conanrc`](https://docs.conan.io/2/reference/config_files/conanrc.html)
sets `conan_home=./conanhome`. Run Conan from this checkout or one of its
subdirectories so it uses the ignored, project-local `conanhome` directory
instead of the user-wide Conan cache. Confirm with:

```bash
conan config home
```

The expected result is `<checkout>/conanhome`. This `.conanrc` setting takes
precedence over `CONAN_HOME`. Conan documents `.conanrc` as a preview feature;
this project requires Conan 2.28 or newer.

## Install Or Update The Conan Configuration

No custom Conan remote hosts the configuration package yet. Its recipe and
package are handled locally from the sibling repository.

On Linux, create the configuration package, disable remotes for `install-pkg`,
and install the revision already pinned by `conan.lock`:

```bash
conan create ../dicom-dataset-editor-conf -pr:h=../dicom-dataset-editor-conf/profiles/linux-gcc-release -pr:b=../dicom-dataset-editor-conf/profiles/linux-gcc-release
conan remote disable "*"
conan config install-pkg conanconfig.yml --lockfile=conan.lock --force -s os=Linux
```

Use the Windows profile paths and `-s os=Windows` when bootstrapping on Windows.
Explicit profile paths avoid depending on profiles that have not been installed
into a fresh project home yet.

After changing the configuration recipe or its exported files, create it again,
then update the lock before running `config install-pkg`:

```bash
conan lock upgrade-config . --no-remote --lockfile=conan.lock --lockfile-out=conan.lock --update-config-requires=dicom-dataset-editor-conf/0.2.0 -pr:h=../dicom-dataset-editor-conf/profiles/linux-gcc-release -pr:b=../dicom-dataset-editor-conf/profiles/linux-gcc-release
```

Commit `conan.lock` when the configuration recipe revision changes. Do not run
the upgrade merely to initialize a fresh home: recreating identical local recipe
content can update its timestamp without changing its recipe revision.

[`conan lock upgrade-config`](https://docs.conan.io/2/reference/commands/lock/upgrade_config.html)
updates only the lockfile; it does not activate the configuration.
[`conan config install-pkg`](https://docs.conan.io/2/reference/commands/config.html#conan-config-install-pkg)
performs that second step. The latter always checks enabled servers for the
latest version or revision and has no `--no-remote` option, so disabling remotes
is required for a cache-only install. The installed configuration defines
ConanCenter again. Run `conan remote disable "*"` after installation when the
remaining build must stay offline.

An offline build succeeds only when every locked recipe, required binary, and
source needed by `--build=missing` is already in `conanhome`. Populate it while
online first, or transfer an archive from a compatible Conan cache with
[`conan cache save` and `conan cache restore`](https://docs.conan.io/2/devops/save_restore.html).
Cache archive transfer is experimental; use the same Conan version on both
sides. Configuration files are not part of that archive, so still run the
configuration-package flow above.

Install the dependencies with the profile that matches your platform.

On Linux:

```powershell
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

After the Conan install, use the checked-in portable CMake presets:

```powershell
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Run the Conan install again after changing the recipe, lockfile, profiles, or dependencies.
CMake configuration is expected to use the generated Conan toolchain; configuring without it requires manually providing all dependencies and `DICOM_EDITOR_DCMTK_DICT_FILE`.

## Install

Install to any prefix with:

```bash
cmake --install build/Release --prefix <your-install-prefix> --config Release
```

The `--config Release` flag is harmless for single-config generators and required
when the build tree uses a multi-config generator such as Visual Studio.

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
