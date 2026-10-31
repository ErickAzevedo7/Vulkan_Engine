#pragma once

#include <core/Types.h>

#include <filesystem>

namespace VKE::Platform {

    // A library loaded while the program runs, unloaded when this is destroyed.
    class DynamicLibrary {
    public:
        // Null, logged, if the library can't be loaded.
        static Iunique<DynamicLibrary> Load(const std::filesystem::path& file);

        ~DynamicLibrary();

        DynamicLibrary(const DynamicLibrary&) = delete;
        DynamicLibrary& operator=(const DynamicLibrary&) = delete;

        // Null if the library doesn't export `name`.
        void* GetSymbol(const char* name) const;

    private:
        explicit DynamicLibrary(void* handle) : m_Handle(handle) {}

        void* m_Handle;
    };

} // namespace VKE::Platform
