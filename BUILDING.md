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

The configuration package is published in the project Artifactory Conan remote.
The checked-in `conan.lock` pins the exact configuration recipe revision. Keep
the remote URL in a local environment variable or secret-managed setup; do not
commit it to this repository.

On Linux:

```bash
export CONAN_ARTIFACTORY_URL="<project Artifactory Conan repository URL>"
conan remote add myartifactory "$CONAN_ARTIFACTORY_URL" --force
conan remote login myartifactory <username>
conan config install-pkg conanconfig.yml --lockfile=conan.lock --force -s os=Linux
conan remote add myartifactory "$CONAN_ARTIFACTORY_URL" --force
```

On Windows, run the equivalent commands from an x64 Native Tools Command
Prompt:

```batch
set "CONAN_ARTIFACTORY_URL=<project Artifactory Conan repository URL>"
conan remote add myartifactory "%CONAN_ARTIFACTORY_URL%" --force
conan remote login myartifactory <username>
conan config install-pkg conanconfig.yml --lockfile=conan.lock --force -s os=Windows
conan remote add myartifactory "%CONAN_ARTIFACTORY_URL%" --force
```

Authenticate with the Artifactory credentials provided for this project. The
configuration package installs the Release, Debug, and optional Ninja Debug
profiles for both Linux/GCC and Windows/MSVC, plus Linux/GCC ASan and TSan
profiles. The second `remote add` restores the user-local Artifactory remote if
the configuration package replaces the Conan remote list.

After changing the configuration recipe or its exported files, publish the new
package to `myartifactory`, update the lock, then run `config install-pkg`:

```bash
conan lock upgrade-config . --lockfile=conan.lock --lockfile-out=conan.lock --update-config-requires=dicom-dataset-editor-conf/0.2.1 -pr:h=linux-gcc-release -pr:b=linux-gcc-release
```

On Windows, use the same command with the Windows profile:

```batch
conan lock upgrade-config . --lockfile=conan.lock --lockfile-out=conan.lock --update-config-requires=dicom-dataset-editor-conf/0.2.1 -pr:h=windows-msvc-release -pr:b=windows-msvc-release
```

The upgrade command checks the configured Artifactory remote for the latest
recipe revision and updates only `conan.lock`; `config install-pkg` activates
that locked revision in the project-local Conan home. Commit `conan.lock`
whenever the configuration recipe revision changes.

[`conan lock upgrade-config`](https://docs.conan.io/2/reference/commands/lock/upgrade_config.html)
updates only the lockfile; it does not activate the configuration.
[`conan config install-pkg`](https://docs.conan.io/2/reference/commands/config.html#conan-config-install-pkg)
performs that second step. It requires the Artifactory remote during the initial
installation; run `conan remote disable "*"` after installation when a
subsequent cache-only build is required.

## Working Without Artifactory

Artifactory is not required after the locked configuration package and all
dependency binaries are available locally. The configuration package revision
from `conan.lock` must already be in the Conan cache; otherwise use the local
configuration-repository fallback below.

### Use an existing Conan cache

Disable remotes while installing the cached configuration package. The package
may install its own remote definitions, so disable them again before an offline
build:

On Linux:

```bash
conan remote disable "*"
conan config install-pkg conanconfig.yml --lockfile=conan.lock --force -s os=Linux
conan remote disable "*"
```

On Windows, run the equivalent commands from an x64 Native Tools Command
Prompt:

```batch
conan remote disable "*"
conan config install-pkg conanconfig.yml --lockfile=conan.lock --force -s os=Windows
conan remote disable "*"
```

Dependency caches can be transferred from a compatible Conan installation with
[`conan cache save` and `conan cache restore`](https://docs.conan.io/2/devops/save_restore.html).
Cache archive transfer is experimental; use the same Conan version on both
sides. Configuration files are not part of that archive, so the configuration
package must also be present in the local Conan cache.

### Rebuild the configuration package locally

If the configuration package is not cached but the sibling
`dicom-dataset-editor-conf` repository is available, create and install a local
copy without contacting a remote. Use a temporary lockfile so the checked-in
Artifactory lock remains unchanged.

On Linux:

```bash
conan create ../dicom-dataset-editor-conf -pr:h=../dicom-dataset-editor-conf/profiles/linux-gcc-release -pr:b=../dicom-dataset-editor-conf/profiles/linux-gcc-release
conan lock upgrade-config . --no-remote --lockfile=conan.lock --lockfile-out=build/offline-conan.lock --update-config-requires=dicom-dataset-editor-conf/0.2.1 -pr:h=../dicom-dataset-editor-conf/profiles/linux-gcc-release -pr:b=../dicom-dataset-editor-conf/profiles/linux-gcc-release
conan config install-pkg conanconfig.yml --lockfile=build/offline-conan.lock --force -s os=Linux
conan remote disable "*"
```

On Windows, use the equivalent commands with the Windows release profile:

```batch
conan create ..\dicom-dataset-editor-conf -pr:h=..\dicom-dataset-editor-conf\profiles\windows-msvc-release -pr:b=..\dicom-dataset-editor-conf\profiles\windows-msvc-release
conan lock upgrade-config . --no-remote --lockfile=conan.lock --lockfile-out=build\offline-conan.lock --update-config-requires=dicom-dataset-editor-conf/0.2.1 -pr:h=..\dicom-dataset-editor-conf\profiles\windows-msvc-release -pr:b=..\dicom-dataset-editor-conf\profiles\windows-msvc-release
conan config install-pkg conanconfig.yml --lockfile=build\offline-conan.lock --force -s os=Windows
conan remote disable "*"
```

Do not commit the temporary offline lockfile. It can point to a local recipe
revision that is not available to other checkouts.

An offline build succeeds only when every locked recipe, required binary, and
source needed by the selected build mode is already in Conan's configured
package storage. Populate it while online first, or transfer an archive from a
compatible Conan cache.

## GitHub Actions And Artifactory CI

The Conan workflows are currently disabled with `on: []` while the Artifactory
repositories, fork upstream, and credentials are being prepared. When ready,
restore the event triggers in the workflow files and configure the variables,
secrets, and protected environment described below.

The repository workflow in [`.github/workflows/conan-ci.yml`](.github/workflows/conan-ci.yml)
is designed to build and test the application on pull requests and pushes to
`main`. Normal CI uses `--build=never`: dependencies must already be available
in Artifactory.
Only the trusted `main` job publishes the application package.

The separate, manually dispatched
[`.github/workflows/conan-package-update.yml`](.github/workflows/conan-package-update.yml)
workflow is the exception. It uses `--build=missing`, records the dependency
graph, and uploads every binary built during that run. Use it after changing
the lockfile or otherwise deliberately updating package binaries. Packages
already fetched from the ConanCenter fork are retained by Artifactory's Conan
remote cache; packages built by this workflow are uploaded to the project
local repository.

### Artifactory repositories

Use separate Artifactory repositories for reading and publishing:

1. Keep or create a local Conan repository such as `conan-ci-local`. This is
   the write destination for first-party packages and the existing
   `dicom-dataset-editor-conf` configuration package.
2. Create a Conan remote repository that proxies your ConanCenter fork. Use
   the fork's Conan API endpoint, for example
   `https://<your-conancenter-fork-host>/api/conan/<remote-repository>`;
   do not use the Git repository URL. Keep the fork synchronized with
   ConanCenter initially, then apply any controlled recipe patches there.
3. Create a Conan virtual repository such as `conan-ci` containing
   `conan-ci-local` and the ConanCenter-fork proxy. Set the local repository as the
   virtual repository's default deployment target only if you want to deploy
   through the virtual URL; this workflow uses the local URL explicitly.

The GitHub secrets should contain these two URLs, without committing either
one to the repository:

- `CONAN_READ_REMOTE_URL`: the virtual repository URL used for package and
  configuration reads.
- `CONAN_PUBLISH_REMOTE_URL`: the local repository URL used only by the publish
  job.

Artifactory repository creation requires Admin or Project Admin permissions. The
[JFrog Conan repository guide](https://docs.jfrog.com/artifactory/docs/conan-repositories)
covers the UI flow.

### Users and permission targets

Create two non-admin service identities. Prefer a group-based permission target
if several developers will use the local account; never share an administrator
credential.

1. Create `dicom-editor-dev` for local development. Grant it `READ` only on
   `conan-ci` (and therefore the packages exposed by that virtual repository).
   Do not grant deploy, delete, manage, or admin permissions.
2. Create `dicom-editor-ci` for GitHub Actions. Grant it `READ` on `conan-ci`
   and `READ`, `DEPLOY/CACHE`, and `ANNOTATE` on `conan-ci-local`. Do not grant
   delete, manage, repository-administration, or platform-administration
   permissions.
3. Generate a separate short-lived or expiring identity token for each user.
   Store the tokens in a password manager and rotate them independently. JFrog
   documents identity-token generation in its
   [identity-token guide](https://docs.jfrog.com/user-management/docs/identity-tokens).
4. Configure an Artifactory permission target for these repository assignments.
   Permission targets control both repository scope and actions; see the
   [JFrog permission-target guide](https://docs.jfrog.com/artifactory/docs/permission-targets).

The CI user is the only identity that can upload. The workflow uses the
developer read-only credentials for verification and the CI credentials only in
the protected publish job.

### GitHub configuration

Add repository secrets for the read-only remote:

```text
CONAN_READ_REMOTE_URL=https://<host>/artifactory/api/conan/conan-ci
```

Add repository secrets for read-only verification:

```text
CONAN_DEV_USERNAME=dicom-editor-dev
CONAN_DEV_TOKEN=<developer identity token>
```

Add environment secrets under a protected `conan-publish` environment:

```text
CONAN_PUBLISH_REMOTE_URL=https://<host>/artifactory/api/conan/conan-ci-local
CONAN_CI_USERNAME=dicom-editor-ci
CONAN_CI_TOKEN=<CI identity token>
```

Restrict the `conan-publish` environment to the `main` branch and require a
reviewer if the repository's release policy calls for manual approval. Pull
requests from forks cannot receive repository secrets; validate them from a
trusted branch or configure an appropriate public read-only dependency path.

For local use, developers add `CONAN_READ_REMOTE_URL` as their private local
environment value and log in with `dicom-editor-dev`. Normal developer installs
use `--build=never`; missing packages should be supplied by the package-update
workflow rather than by a developer upload. No CI upload command should be
part of the developer workflow.

Install dependencies with profiles matching the intended configuration. Host
libraries use that configuration; build tools such as CMake and cppcheck stay
in Release.

For the final optimized Linux executable:

```bash
conan install . --build=never --lockfile=conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release -c tools.build:skip_test=True
```

For daily Linux development with assertions and debug information:

```bash
conan install . --build=never --lockfile=conan.lock -pr:h=linux-gcc-debug -pr:b=linux-gcc-release
```

Windows uses the equivalent host profiles while retaining the Release build
profile. Run these from an x64 Native Tools Command Prompt:

```batch
conan install . --build=never --lockfile=conan.lock -pr:h=windows-msvc-release -pr:b=windows-msvc-release -c tools.build:skip_test=True
conan install . --build=never --lockfile=conan.lock -pr:h=windows-msvc-debug -pr:b=windows-msvc-release
```

Default Debug uses Unix Makefiles on Linux and Visual Studio on Windows. Install
the optional Ninja Debug profile for `dev-ninja`, `quality-checks`, and
`all-checks`:

```bash
# Linux
conan install . --build=never --lockfile=conan.lock -pr:h=linux-gcc-debug-ninja -pr:b=linux-gcc-release

# Windows x64 Native Tools Command Prompt
conan install . --build=never --lockfile=conan.lock -pr:h=windows-msvc-debug-ninja -pr:b=windows-msvc-release
```

Linux sanitizer builds use dedicated Ninja profiles. The sanitizer is a Conan
compiler setting, so sanitized dependencies have distinct package IDs and are
rebuilt as needed instead of being mixed with ordinary Debug binaries:

```bash
conan install . --build=never --lockfile=conan.lock -pr:h=linux-gcc-asan-ninja -pr:b=linux-gcc-release
conan install . --build=never --lockfile=conan.lock -pr:h=linux-gcc-tsan-ninja -pr:b=linux-gcc-release
```

The first install enables AddressSanitizer and UndefinedBehaviorSanitizer; the
second enables ThreadSanitizer and UndefinedBehaviorSanitizer. They generate
independent toolchains under `build/Ninja-Debug-ASan/generators` and
`build/Ninja-Debug-TSan/generators`. Expect the first install for each profile
to build dependency binaries from source. See [HACKING.md](HACKING.md#runtime-analysis)
for the one-command test workflows.

Release, default Debug, and Ninja Debug installs generate toolchains under
`build/Release/generators`, `build/Debug/generators`, and
`build/Ninja-Debug/generators`, respectively. They coexist; switching generators
does not overwrite another toolchain. Conan also passes the DCMTK dictionary
location to CMake; the build embeds it, so the installed application does not
depend on the Conan cache or a separate data file. Pinned CMake and cppcheck,
plus Ninja when selected, are installed in Conan's Release build context and
recorded by the lockfile.

Catch2 is a host-context `test_requires` dependency. Test-enabled Debug,
quality, sanitizer, and `conan create` builds receive it automatically. Release
installs use `tools.build:skip_test=True`, which disables `BUILD_TESTING` and
omits Catch2 from the dependency graph.

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

On Windows, run from an x64 Native Tools Command Prompt:

```batch
call build\Release\generators\conanbuild.bat
cmake --preset production
cmake --build --preset production
```

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
conan create . --build=never --lockfile=conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release -c tools.build:skip_test=False
```

Use `-c tools.build:skip_test=True` when package creation should skip tests.
