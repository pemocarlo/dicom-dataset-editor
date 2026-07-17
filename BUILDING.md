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

On Windows, the installed configuration automatically stores packages in a
short, unique `C:\conan_<hash>` path. Profiles and configuration remain in the
project-local home; the separate storage avoids a ConanCenter Meson wrapper
that cannot handle spaces in cache paths. Other platforms use Conan's default
storage under `conanhome`.

## Install Or Update The Conan Configuration

No custom Conan remote hosts the configuration package yet. Its recipe and
package are handled locally from an up-to-date sibling repository.

Create the configuration package with the bootstrap profile for the current
platform, disable remotes for `install-pkg`, and install the revision already
pinned by `conan.lock`.

On Linux:

```bash
conan create ../dicom-dataset-editor-conf -pr:h=../dicom-dataset-editor-conf/profiles/linux-gcc-release -pr:b=../dicom-dataset-editor-conf/profiles/linux-gcc-release
conan remote disable "*"
conan config install-pkg conanconfig.yml --lockfile=conan.lock --force -s os=Linux
```

On Windows, run the equivalent commands from an x64 Native Tools Command
Prompt:

```batch
conan create ..\dicom-dataset-editor-conf -pr:h=..\dicom-dataset-editor-conf\profiles\windows-msvc-release -pr:b=..\dicom-dataset-editor-conf\profiles\windows-msvc-release
conan remote disable "*"
conan config install-pkg conanconfig.yml --lockfile=conan.lock --force -s os=Windows
```

Explicit profile paths avoid depending on profiles that have not been installed
into a fresh project home yet. The package installs Release, Debug, and optional
Ninja Debug profiles for both Linux/GCC and Windows/MSVC.

After changing the configuration recipe or its exported files, create it again,
then update the lock before running `config install-pkg`:

```bash
conan lock upgrade-config . --no-remote --lockfile=conan.lock --lockfile-out=conan.lock --update-config-requires=dicom-dataset-editor-conf/0.2.0 -pr:h=../dicom-dataset-editor-conf/profiles/linux-gcc-release -pr:b=../dicom-dataset-editor-conf/profiles/linux-gcc-release
```

On Windows, use the same command with the bootstrap profile paths from the
Windows block above:

```batch
conan lock upgrade-config . --no-remote --lockfile=conan.lock --lockfile-out=conan.lock --update-config-requires=dicom-dataset-editor-conf/0.2.0 -pr:h=..\dicom-dataset-editor-conf\profiles\windows-msvc-release -pr:b=..\dicom-dataset-editor-conf\profiles\windows-msvc-release
```

If `config install-pkg` cannot find the package after `conan create` succeeds,
the lock likely pins another recipe revision. Run the matching
`lock upgrade-config` command above and retry the install.

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
source needed by `--build=missing` is already in Conan's configured package
storage. Populate it while online first, or transfer an archive from a
compatible Conan cache with
[`conan cache save` and `conan cache restore`](https://docs.conan.io/2/devops/save_restore.html).
Cache archive transfer is experimental; use the same Conan version on both
sides. Configuration files are not part of that archive, so still run the
configuration-package flow above.

Install dependencies with profiles matching the intended configuration. Host
libraries use that configuration; build tools such as CMake and cppcheck stay
in Release.

For the final optimized Linux executable:

```bash
conan install . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release
```

For daily Linux development with assertions and debug information:

```bash
conan install . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-debug -pr:b=linux-gcc-release
```

Windows uses the equivalent host profiles while retaining the Release build
profile. Run these from an x64 Native Tools Command Prompt:

```batch
conan install . --build=missing --lockfile=conan.lock -pr:h=windows-msvc-release -pr:b=windows-msvc-release
conan install . --build=missing --lockfile=conan.lock -pr:h=windows-msvc-debug -pr:b=windows-msvc-release
```

Default Debug uses Unix Makefiles on Linux and Visual Studio on Windows. Install
the optional Ninja Debug profile for `dev-ninja`, `quality-checks`, and
`all-checks`:

```bash
# Linux
conan install . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-debug-ninja -pr:b=linux-gcc-release

# Windows x64 Native Tools Command Prompt
conan install . --build=missing --lockfile=conan.lock -pr:h=windows-msvc-debug-ninja -pr:b=windows-msvc-release
```

Release, default Debug, and Ninja Debug installs generate toolchains under
`build/Release/generators`, `build/Debug/generators`, and
`build/Ninja-Debug/generators`, respectively. They coexist; switching generators
does not overwrite another toolchain. Conan also passes the DCMTK dictionary
location to CMake; the build embeds it, so the installed application does not
depend on the Conan cache or a separate data file. Pinned CMake and cppcheck,
plus Ninja when selected, are installed in Conan's Release build context and
recorded by the lockfile.

In-source builds are not supported.

## Production Build

`production` is the project-facing name for CMake's standard optimized Release
configuration. It builds the executable delivered to users and omits tests and
developer tooling:

```bash
source build/Release/generators/conanbuild.sh
cmake --preset production
cmake --build --preset production
```

On Windows cmd.exe, activate `build\Release\generators\conanbuild.bat` instead.
Activation selects Conan's pinned CMake; a system CMake 4.0 or newer also works.

For a Debug build with tests and fast daily checks, use the workflow documented
in [HACKING.md](HACKING.md). Run the matching Conan install again after changing
the recipe, lockfile, profiles, or dependencies. Configuring without a generated
toolchain requires manually providing all dependencies and
`DICOM_EDITOR_DCMTK_DICT_FILE`.

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
