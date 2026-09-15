#include "platform/FileSystem.h"

#include <core/Log.h>
#include <fstream>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#endif

namespace VKE::Platform::FileSystem {

    std::filesystem::path GetExecutableDirectory() {
#ifdef _WIN32
        std::wstring buffer(MAX_PATH, L'\0');
        for (;;) {
            DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0) {
                VKE_CORE_ERROR("FileSystem::GetExecutableDirectory: GetModuleFileNameW failed");
                return std::filesystem::current_path();
            }
            if (length < buffer.size()) {
                buffer.resize(length);
                break;
            }
            buffer.resize(buffer.size() * 2);
        }
        return std::filesystem::path(buffer).parent_path();
#else
        std::error_code ec;
        std::filesystem::path exe = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (ec) {
            VKE_CORE_ERROR("FileSystem::GetExecutableDirectory: {}", ec.message());
            return std::filesystem::current_path();
        }
        return exe.parent_path();
#endif
    }

    std::vector<Byte> ReadFileBytes(const std::string& path) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            VKE_CORE_ERROR("FileSystem::ReadFileBytes: failed to open '{}'", path);
            return {};
        }

        size_t size = static_cast<size_t>(file.tellg());
        std::vector<Byte> buffer(size);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(size));
        file.close();

        return buffer;
    }

    std::string ReadFileText(const std::string& path) {
        std::ifstream file(path, std::ios::ate);
        if (!file.is_open()) {
            VKE_CORE_ERROR("FileSystem::ReadFileText: failed to open '{}'", path);
            return {};
        }

        size_t size = static_cast<size_t>(file.tellg());
        std::string buffer(size, '\0');

        file.seekg(0);
        file.read(buffer.data(), static_cast<std::streamsize>(size));
        file.close();

        return buffer;
    }

    bool Exists(const std::string& path) {
        std::error_code ec;
        return std::filesystem::exists(path, ec);
    }

} // namespace VKE::Platform::FileSystem
