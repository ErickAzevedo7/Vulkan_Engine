#include "ProjectTemplates.h"

#include <core/Log.h>
#include <platform/FileSystem.h>
#include <project/Project.h>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <fstream>

namespace VKE::Editor {

    namespace {
        constexpr const char* kTemplateFile = "Template.yaml";

        std::string ReadString(const YAML::Node& root, const char* key) {
            YAML::Node value = root[key];
            return value && value.IsScalar() ? value.Scalar() : std::string();
        }
    } // namespace

    std::vector<ProjectTemplate> FindProjectTemplates() {
        std::vector<ProjectTemplate> templates;
        std::filesystem::path folder = Platform::FileSystem::GetExecutableDirectory() / "Templates";
        std::error_code ec;
        for (auto it = std::filesystem::directory_iterator(folder, ec);
             !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
            if (!it->is_directory(ec)) continue;
            std::ifstream stream(it->path() / kTemplateFile, std::ios::binary);
            if (!stream) continue;

            YAML::Node root;
            try {
                root = YAML::Load(stream);
            } catch (const YAML::Exception& error) {
                VKE_CORE_WARN("Editor: template '{}' can't be read: {}", it->path().u8string(), error.what());
                continue;
            }
            ProjectTemplate projectTemplate;
            projectTemplate.Folder = it->path();
            projectTemplate.Name = ReadString(root, "Name");
            if (projectTemplate.Name.empty()) projectTemplate.Name = it->path().filename().u8string();
            projectTemplate.Description = ReadString(root, "Description");
            projectTemplate.StartScene = Assets::AssetID::Parse(ReadString(root, "StartScene"));
            templates.push_back(std::move(projectTemplate));
        }
        if (templates.empty()) VKE_CORE_WARN("Editor: no project templates in '{}'", folder.u8string());

        std::sort(templates.begin(), templates.end(),
                  [](const ProjectTemplate& a, const ProjectTemplate& b) { return a.Name < b.Name; });
        return templates;
    }

    std::string CheckProjectName(const std::string& name) {
        if (name.empty()) return "Enter a name.";
        if (name.size() > 64) return "Use 64 characters or fewer.";
        for (char c : name) {
            if (static_cast<unsigned char>(c) < 32 || std::string("<>:\"/\\|?*").find(c) != std::string::npos) {
                return "Names can't have < > : \" / \\ | ? *";
            }
        }
        if (name.back() == '.' || name.back() == ' ' || name.front() == ' ') {
            return "Names can't start with a space or end with a space or a dot.";
        }

        // Names Windows keeps for devices, with or without an extension.
        std::string base = name.substr(0, name.find('.'));
        std::transform(base.begin(), base.end(), base.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        static const char* const kReserved[] = {"CON", "PRN", "AUX", "NUL"};
        bool reserved = std::find(std::begin(kReserved), std::end(kReserved), base) != std::end(kReserved);
        reserved |= base.size() == 4 && (base.rfind("COM", 0) == 0 || base.rfind("LPT", 0) == 0) &&
                    base[3] >= '1' && base[3] <= '9';
        if (reserved) return "Windows keeps that name for itself.";
        return {};
    }

    std::optional<Project::Project> CreateProject(const ProjectTemplate& projectTemplate,
                                                  const std::filesystem::path& folder, const std::string& name) {
        std::error_code ec;
        if (std::filesystem::exists(folder, ec) && !std::filesystem::is_empty(folder, ec)) {
            VKE_CORE_ERROR("Editor: '{}' already exists and isn't empty", folder.u8string());
            return std::nullopt;
        }
        std::filesystem::create_directories(folder, ec);
        if (ec) {
            VKE_CORE_ERROR("Editor: couldn't create '{}': {}", folder.u8string(), ec.message());
            return std::nullopt;
        }

        for (auto it = std::filesystem::directory_iterator(projectTemplate.Folder, ec);
             !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
            if (it->path().filename() == kTemplateFile) continue;
            std::error_code copyError;
            std::filesystem::copy(it->path(), folder / it->path().filename(),
                                  std::filesystem::copy_options::recursive, copyError);
            if (copyError) {
                VKE_CORE_ERROR("Editor: couldn't copy '{}' into the new project: {}", it->path().u8string(),
                               copyError.message());
                return std::nullopt;
            }
        }

        std::optional<Project::Project> project = Project::Project::Create(folder, name, projectTemplate.StartScene);
        if (project) VKE_CORE_INFO("Editor: created project '{}' in '{}'", name, folder.u8string());
        return project;
    }

} // namespace VKE::Editor
