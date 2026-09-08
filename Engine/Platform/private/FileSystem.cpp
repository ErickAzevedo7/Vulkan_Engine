#include "platform/FileSystem.h"

#include <core/Log.h>
#include <fstream>
#include <filesystem>

namespace VKE::Platform::FileSystem {

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
