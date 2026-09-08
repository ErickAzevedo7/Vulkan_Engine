#pragma once

#include <optional>
#include <string>

// Native Open/Save file dialogs. Not implemented until the Editor
// (Phase 6). Calling these before then logs a warning and returns nullopt.

namespace VKE::Platform::FileDialog {

    std::optional<std::string> OpenFile(const char* filterName, const char* filterPattern);
    std::optional<std::string> SaveFile(const char* filterName, const char* filterPattern);

} // namespace VKE::Platform::FileDialog
