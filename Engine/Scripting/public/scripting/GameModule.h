#pragma once

#include <core/Types.h>

#include <filesystem>
#include <vector>

namespace VKE::Platform {
    class DynamicLibrary;
} // namespace VKE::Platform

namespace VKE::Scripting {

    struct ScriptType;

    // A game's DLL and the script types it brought to the ComponentRegistry. Objects of those types
    // run code from the DLL, so scenes that have them must be gone before this is unloaded.
    class GameModule {
    public:
        GameModule();
        ~GameModule();

        GameModule(const GameModule&) = delete;
        GameModule& operator=(const GameModule&) = delete;

        // Loads `library` and registers its scripts. Call after the engine's components are
        // registered. It runs a copy, kept in `workFolder`, so the file can be rebuilt meanwhile.
        // False if there's no file, which is normal for a project with no scripts built yet, or
        // if it can't be loaded, which is logged.
        bool Load(const std::filesystem::path& library, const std::filesystem::path& workFolder);
        // Takes the scripts off the registry, as missing types, and unloads the DLL.
        void Unload();

        bool IsLoaded() const { return m_Library != nullptr; }

    private:
        Iunique<Platform::DynamicLibrary> m_Library;
        std::vector<Iunique<ScriptType>> m_Types;
        std::filesystem::path m_RunningCopy;
    };

} // namespace VKE::Scripting
