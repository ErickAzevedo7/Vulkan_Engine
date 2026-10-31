#include "platform/DynamicLibrary.h"

#include <core/Log.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace VKE::Platform {

    Iunique<DynamicLibrary> DynamicLibrary::Load(const std::filesystem::path& file) {
        HMODULE handle = LoadLibraryW(file.c_str());
        if (!handle) {
            VKE_CORE_ERROR("DynamicLibrary: couldn't load '{}' (error {})", file.u8string(), GetLastError());
            return nullptr;
        }
        return Iunique<DynamicLibrary>(new DynamicLibrary(handle));
    }

    DynamicLibrary::~DynamicLibrary() {
        FreeLibrary(static_cast<HMODULE>(m_Handle));
    }

    void* DynamicLibrary::GetSymbol(const char* name) const {
        return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_Handle), name));
    }

} // namespace VKE::Platform
