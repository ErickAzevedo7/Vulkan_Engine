#pragma once

#include <core/Types.h>

#include <filesystem>
#include <string>

// The programs of whoever writes a game's code, found on their machine.

namespace VKE::Editor {

    // The newest Visual Studio with the C++ compiler; empty if there's none.
    std::filesystem::path FindVisualStudio();

    // `program` in the folders of `pathVariable`, a list like the PATH variable holds; empty if
    // none has it.
    std::filesystem::path FindOnPath(const std::string& pathVariable, const std::string& program);

    // Opens `file` at `line` in VS Code if it's on the PATH, or else in Visual Studio. Logged if
    // neither is there.
    void OpenInCodeEditor(const std::filesystem::path& file, u32 line);

} // namespace VKE::Editor
