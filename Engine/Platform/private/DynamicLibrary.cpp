#include "platform/DynamicLibrary.h"

#include <core/Log.h>

// Systems without a backend yet. Windows uses private/windows/WindowsDynamicLibrary.cpp.

namespace VKE::Platform {

    Iunique<DynamicLibrary> DynamicLibrary::Load(const std::filesystem::path& file) {
        VKE_CORE_WARN("DynamicLibrary: can't load '{}' on this system", file.u8string());
        return nullptr;
    }

    DynamicLibrary::~DynamicLibrary() = default;

    void* DynamicLibrary::GetSymbol(const char*) const {
        return nullptr;
    }

} // namespace VKE::Platform
