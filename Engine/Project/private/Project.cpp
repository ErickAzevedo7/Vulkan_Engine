#include "project/Project.h"

#include <core/Log.h>
#include <core/Version.h>
#include <platform/FileSystem.h>

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace VKE::Project {

    namespace {
        std::string ReadString(const YAML::Node& root, const char* key) {
            YAML::Node value = root[key];
            return value && value.IsScalar() ? value.Scalar() : std::string();
        }
    } // namespace

    std::filesystem::path Project::FindProjectFile(const std::filesystem::path& folder) {
        std::error_code ec;
        for (auto it = std::filesystem::directory_iterator(folder, ec);
             !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
            if (it->is_regular_file(ec) && it->path().extension() == kExtension) return it->path();
        }
        return {};
    }

    std::optional<Project> Project::Open(const std::filesystem::path& folderOrFile) {
        std::error_code ec;
        std::filesystem::path file = folderOrFile;
        if (std::filesystem::is_directory(folderOrFile, ec)) {
            file = FindProjectFile(folderOrFile);
            if (file.empty()) {
                VKE_CORE_ERROR("Project: '{}' has no {} file", folderOrFile.u8string(), kExtension);
                return std::nullopt;
            }
        }

        std::ifstream stream(file, std::ios::binary);
        if (!stream) {
            VKE_CORE_ERROR("Project: couldn't open '{}'", file.u8string());
            return std::nullopt;
        }
        YAML::Node root;
        try {
            root = YAML::Load(stream);
        } catch (const YAML::Exception& error) {
            VKE_CORE_ERROR("Project: '{}': {}", file.u8string(), error.what());
            return std::nullopt;
        }
        if (!root.IsMap()) {
            VKE_CORE_ERROR("Project: '{}' is not a project file", file.u8string());
            return std::nullopt;
        }

        Project project;
        project.m_File = std::filesystem::weakly_canonical(file, ec);
        if (ec) project.m_File = file;
        project.m_Folder = project.m_File.parent_path();
        project.m_Name = ReadString(root, "Name");
        if (project.m_Name.empty()) project.m_Name = file.stem().u8string();
        project.m_StartScene = Assets::AssetID::Parse(ReadString(root, "StartScene"));

        std::string version = ReadString(root, "EngineVersion");
        if (!version.empty() && version != kEngineVersion) {
            VKE_CORE_WARN("Project: '{}' was saved by engine {}; this is {}", project.m_Name, version, kEngineVersion);
        }
        return project;
    }

    std::optional<Project> Project::Create(const std::filesystem::path& folder, const std::string& name,
                                           Assets::AssetID startScene) {
        Project project;
        std::error_code ec;
        project.m_Folder = std::filesystem::weakly_canonical(folder, ec);
        if (ec) project.m_Folder = folder;
        project.m_File = project.m_Folder / std::filesystem::u8path(name + kExtension);
        project.m_Name = name;
        project.m_StartScene = startScene;
        if (!project.Save()) return std::nullopt;
        return project;
    }

    bool Project::Save() const {
        YAML::Emitter out;
        out << YAML::Comment("VKE project. Open its folder in the editor.");
        out << YAML::BeginMap;
        out << YAML::Key << "Name" << YAML::Value << YAML::DoubleQuoted << m_Name;
        out << YAML::Key << "EngineVersion" << YAML::Value << YAML::DoubleQuoted << kEngineVersion;
        if (m_StartScene.IsValid()) {
            out << YAML::Key << "StartScene" << YAML::Value << YAML::DoubleQuoted << m_StartScene.ToString();
        }
        out << YAML::EndMap;

        std::ofstream stream(m_File, std::ios::binary | std::ios::trunc);
        stream << out.c_str() << '\n';
        if (!stream) {
            VKE_CORE_ERROR("Project: couldn't write '{}'", m_File.u8string());
            return false;
        }
        return true;
    }

    std::vector<Assets::AssetRoot> Project::GetAssetRoots() const {
        return {
            {"assets", GetAssetsFolder(), false},
            {"Engine", Platform::FileSystem::GetEngineContentDirectory(), true},
        };
    }

    std::filesystem::path FindProjectArgument(const std::vector<std::string>& arguments) {
        for (size_t i = 0; i + 1 < arguments.size(); ++i) {
            if (arguments[i] == "--project") return std::filesystem::u8path(arguments[i + 1]);
        }
        return {};
    }

} // namespace VKE::Project
