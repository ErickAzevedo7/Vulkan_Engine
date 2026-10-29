#pragma once

#include <core/Types.h>
#include <filesystem>
#include <string>
#include <vector>

// Generic, backend-independent file I/O.

namespace VKE::Platform::FileSystem {

    // The running executable's file, and the directory containing it.
    std::filesystem::path GetExecutablePath();
    std::filesystem::path GetExecutableDirectory();

    // The engine's own assets and fonts: a "Content" folder next to the executable.
    std::filesystem::path GetEngineContentDirectory();

    // Where the engine keeps the user's settings, like the recent projects: a "VKE" folder in
    // the system's per-user data folder (%APPDATA% on Windows), created if missing.
    std::filesystem::path GetUserDataDirectory();

    // The user's documents folder.
    std::filesystem::path GetDocumentsDirectory();

    // Reads an entire binary file into memory.
    // Returns an empty vector and logs an error if the file cannot be opened.
    std::vector<Byte> ReadFileBytes(const std::string& path);

    // Reads an entire text file into a string.
    // Returns an empty string and logs an error if the file cannot be opened.
    std::string ReadFileText(const std::string& path);

    bool Exists(const std::string& path);

} // namespace VKE::Platform::FileSystem
