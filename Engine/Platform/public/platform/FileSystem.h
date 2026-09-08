#pragma once

#include <core/Types.h>
#include <string>
#include <vector>

// Generic, backend-independent file I/O.
//
// TODO: overlaps with `Utils::readFile` (Vulkan_Engine/Vulkan_Engine/core/Utils.*);
// migrate remaining callers here as the RHI migration touches them.

namespace VKE::Platform::FileSystem {

    // Reads an entire binary file into memory.
    // Returns an empty vector and logs an error if the file cannot be opened.
    std::vector<Byte> ReadFileBytes(const std::string& path);

    // Reads an entire text file into a string.
    // Returns an empty string and logs an error if the file cannot be opened.
    std::string ReadFileText(const std::string& path);

    bool Exists(const std::string& path);

} // namespace VKE::Platform::FileSystem
