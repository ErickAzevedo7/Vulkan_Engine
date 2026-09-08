#include "platform/FileDialog.h"

#include <core/Log.h>

namespace VKE::Platform::FileDialog {

    std::optional<std::string> OpenFile(const char* filterName, const char* filterPattern) {
        (void)filterName;
        (void)filterPattern;
        VKE_CORE_WARN("FileDialog::OpenFile: not implemented until the Editor lands (Phase 6)");
        return std::nullopt;
    }

    std::optional<std::string> SaveFile(const char* filterName, const char* filterPattern) {
        (void)filterName;
        (void)filterPattern;
        VKE_CORE_WARN("FileDialog::SaveFile: not implemented until the Editor lands (Phase 6)");
        return std::nullopt;
    }

} // namespace VKE::Platform::FileDialog
