#pragma once

#include <filesystem>
#include <string>
#include <vector>

// The running program's arguments, and starting other programs.

namespace VKE::Platform::Process {

    // The arguments the program was started with, without the program itself, in UTF-8.
    std::vector<std::string> GetCommandLineArguments();

    // Starts `executable` with `arguments` (UTF-8) and returns without waiting for it.
    // Returns false, logging why, if it couldn't be started.
    bool Launch(const std::filesystem::path& executable, const std::vector<std::string>& arguments);

} // namespace VKE::Platform::Process
