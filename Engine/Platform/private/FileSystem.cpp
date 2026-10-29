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
    #include <shlobj.h>
#else
    #include <cstdlib>
#endif

namespace VKE::Platform::FileSystem {

    std::filesystem::path GetExecutablePath() {
#ifdef _WIN32
        std::wstring buffer(MAX_PATH, L'\0');
        for (;;) {
            DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0) {
                VKE_CORE_ERROR("FileSystem::GetExecutablePath: GetModuleFileNameW failed");
                return std::filesystem::current_path();
            }
            if (length < buffer.size()) {
                buffer.resize(length);
                break;
            }
            buffer.resize(buffer.size() * 2);
        }
        return std::filesystem::path(buffer);
#else
        std::error_code ec;
        std::filesystem::path exe = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (ec) {
            VKE_CORE_ERROR("FileSystem::GetExecutablePath: {}", ec.message());
            return std::filesystem::current_path();
        }
        return exe;
#endif
    }

    std::filesystem::path GetExecutableDirectory() {
        return GetExecutablePath().parent_path();
    }

    std::filesystem::path GetEngineContentDirectory() {
        return GetExecutableDirectory() / "Content";
    }

    namespace {
#ifdef _WIN32
        std::filesystem::path GetKnownFolder(const KNOWNFOLDERID& id) {
            PWSTR folder = nullptr;
            std::filesystem::path path;
            if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &folder))) path = folder;
            CoTaskMemFree(folder);
            return path;
        }
#else
        std::filesystem::path GetHome() {
            const char* home = std::getenv("HOME");
            return home ? std::filesystem::path(home) : std::filesystem::current_path();
        }
#endif
    } // namespace

    std::filesystem::path GetUserDataDirectory() {
#ifdef _WIN32
        std::filesystem::path base = GetKnownFolder(FOLDERID_RoamingAppData);
#else
        const char* dataHome = std::getenv("XDG_DATA_HOME");
        std::filesystem::path base = dataHome ? std::filesystem::path(dataHome) : GetHome() / ".local" / "share";
#endif
        if (base.empty()) {
            VKE_CORE_ERROR("FileSystem::GetUserDataDirectory: the system didn't give a folder");
            base = GetExecutableDirectory();
        }
        std::filesystem::path folder = base / "VKE";
        std::error_code ec;
        std::filesystem::create_directories(folder, ec);
        return folder;
    }

    std::filesystem::path GetDocumentsDirectory() {
#ifdef _WIN32
        std::filesystem::path folder = GetKnownFolder(FOLDERID_Documents);
        return folder.empty() ? std::filesystem::current_path() : folder;
#else
        return GetHome() / "Documents";
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
