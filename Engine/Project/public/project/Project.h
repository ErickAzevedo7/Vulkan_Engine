#pragma once

#include <assets/AssetTypes.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace VKE::Project {

    // A game made with the engine: a folder anywhere on disk with a <Name>.vkproject file and an
    // assets/ folder. The .vkproject holds the name, the engine version that last saved it and
    // the scene the game starts in.
    class Project {
    public:
        static constexpr const char* kExtension = ".vkproject";

        // Reads the project in `folderOrFile`, a project folder or its .vkproject. Nothing, with
        // an error logged, if there's none or it can't be read.
        static std::optional<Project> Open(const std::filesystem::path& folderOrFile);

        // Writes <name>.vkproject into `folder`, which must exist. Nothing, logged, if it fails.
        static std::optional<Project> Create(const std::filesystem::path& folder, const std::string& name,
                                             Assets::AssetID startScene);

        // The first .vkproject in `folder`; empty if it has none.
        static std::filesystem::path FindProjectFile(const std::filesystem::path& folder);

        // Writes the .vkproject. False, logged, if it can't be written.
        bool Save() const;

        const std::string& GetName() const { return m_Name; }
        const std::filesystem::path& GetFolder() const { return m_Folder; }
        const std::filesystem::path& GetFile() const { return m_File; }
        std::filesystem::path GetAssetsFolder() const { return m_Folder / "assets"; }
        // The editor's own files for the project, like its panel layout: a .vke folder, kept
        // out of version control.
        std::filesystem::path GetSettingsFolder() const { return m_Folder / ".vke"; }
        // The game's code: a scripts/ folder, built into a DLL in the .vke folder.
        std::filesystem::path GetScriptsFolder() const { return m_Folder / "scripts"; }
        std::filesystem::path GetGameLibrary() const { return GetSettingsFolder() / "bin" / "Game.dll"; }
        // Where a program keeps the copy of the DLL it runs.
        std::filesystem::path GetGameWorkFolder() const { return GetSettingsFolder() / "run"; }

        // Invalid when the project has none.
        Assets::AssetID GetStartScene() const { return m_StartScene; }
        void SetStartScene(Assets::AssetID scene) { m_StartScene = scene; }

        // The project's assets, where relative asset paths point, then the engine's content,
        // read-only.
        std::vector<Assets::AssetRoot> GetAssetRoots() const;

    private:
        std::string m_Name;
        std::filesystem::path m_Folder;
        std::filesystem::path m_File;
        Assets::AssetID m_StartScene;
    };

    // The folder after "--project" in a program's arguments; empty if there's none.
    std::filesystem::path FindProjectArgument(const std::vector<std::string>& arguments);

} // namespace VKE::Project
