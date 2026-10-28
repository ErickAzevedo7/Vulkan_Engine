#include "platform/FileDialog.h"

#include <core/Log.h>

// Systems without native dialogs yet. Windows uses private/windows/WindowsFileDialog.cpp.

namespace VKE::Platform::FileDialog {

    std::optional<std::filesystem::path> OpenFile(const std::vector<Filter>&, const std::filesystem::path&) {
        VKE_CORE_WARN("FileDialog::OpenFile: no native dialog on this system");
        return std::nullopt;
    }

    std::optional<std::filesystem::path> SaveFile(const std::vector<Filter>&, const std::string&,
                                                  const std::filesystem::path&) {
        VKE_CORE_WARN("FileDialog::SaveFile: no native dialog on this system");
        return std::nullopt;
    }

    std::optional<std::filesystem::path> PickFolder(const std::filesystem::path&) {
        VKE_CORE_WARN("FileDialog::PickFolder: no native dialog on this system");
        return std::nullopt;
    }

} // namespace VKE::Platform::FileDialog
