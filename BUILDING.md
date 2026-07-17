# Building

This project uses Conan profiles from the installed `dicom-dataset-editor-conf` package. On Linux, build with the profile set from that package:

```bash
conan install . --build=missing --lockfile=conan.lock -pr:h=linux-gcc-release -pr:b=linux-gcc-release
```

That command generates `build/Release/generators/CMakePresets.json` and the Conan toolchain.

## Configure And Build

```bash
cmake --preset conan-release
cmake --build --preset conan-release
ctest --preset conan-release
```

## Relocatable Install

Use `DICOM_EDITOR_RELOCATABLE_INSTALL=ON` when you want a per-user install with bundled non-system shared libraries and relative runtime paths:

```bash
cmake --preset conan-release -DDICOM_EDITOR_RELOCATABLE_INSTALL=ON
cmake --build --preset conan-release
cmake --install build/Release --prefix <your-install-prefix>
```

Expected layout:

- `<your-install-prefix>/bin/dicom-dataset-editor`
- Bundled non-system shared libraries under `<your-install-prefix>/lib`
- Runtime data under `<your-install-prefix>/share/dicom-dataset-editor`

System libraries stay system-provided. The executable uses relative runtime paths so the installed prefix can be moved.

## Notes

- `DICOM_EDITOR_RELOCATABLE_INSTALL` defaults to `OFF`.
- Use `--prefix` for one-off installs. It keeps the build tree unchanged.
- If you install under another writable directory, replace `<your-install-prefix>` with that prefix.
