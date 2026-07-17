#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <system_error>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <vector>
#elif defined(_WIN32)
#include <vector>
#include <windows.h>
#endif

namespace dicom_editor {

inline std::filesystem::path executableDirectory() {
#if defined(__linux__)
    std::error_code error;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    return error ? std::filesystem::path{} : executable.parent_path();
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        return {};
    }
    std::error_code error;
    const auto executable = std::filesystem::weakly_canonical(buffer.data(), error);
    return error ? std::filesystem::path{} : executable.parent_path();
#elif defined(_WIN32)
    std::vector<wchar_t> buffer(32768);
    const auto size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size == 0 || size == buffer.size()) {
        return {};
    }
    return std::filesystem::path(std::wstring_view(buffer.data(), size)).parent_path();
#else
    return {};
#endif
}

inline std::filesystem::path installedDataPath([[maybe_unused]] const std::filesystem::path &relativePath) {
#ifdef DICOM_EDITOR_INSTALL_DATADIR
    const auto executableDir = executableDirectory();
    if (!executableDir.empty()) {
        return executableDir / DICOM_EDITOR_INSTALL_DATADIR / relativePath;
    }
#endif
    return {};
}

} // namespace dicom_editor
