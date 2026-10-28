#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Native dialogs for choosing files and folders, modal to the engine window.
// Each returns nothing when the user cancels.

namespace VKE::Platform::FileDialog {

    // One entry of the dialog's file type list, e.g. {"Scene", "*.scene"}.
    // Several patterns are separated by ';' ("*.png;*.jpg").
    struct Filter {
        std::string Name;
        std::string Pattern;
    };

    // `folder` is where the dialog starts; empty lets the system choose.
    std::optional<std::filesystem::path> OpenFile(const std::vector<Filter>& filters,
                                                  const std::filesystem::path& folder = {});

    // Adds the first filter's extension when the typed name has none.
    std::optional<std::filesystem::path> SaveFile(const std::vector<Filter>& filters,
                                                  const std::string& defaultName = {},
                                                  const std::filesystem::path& folder = {});

    std::optional<std::filesystem::path> PickFolder(const std::filesystem::path& folder = {});

} // namespace VKE::Platform::FileDialog
