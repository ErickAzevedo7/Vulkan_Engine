#pragma once

#include <assets/AssetTypes.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace VKE::Project {
    class Project;
} // namespace VKE::Project

namespace VKE::Editor {

    // A starting point for new projects: a folder in Templates/ next to the editor, copied
    // whole into the new project, with a Template.yaml that names and describes it and says
    // which of its scenes the project starts in.
    struct ProjectTemplate {
        std::string Name;
        std::string Description;
        std::filesystem::path Folder;
        Assets::AssetID StartScene;
    };

    // The templates next to the editor, by name.
    std::vector<ProjectTemplate> FindProjectTemplates();

    // Empty when `name` can name a project and its folder; otherwise the reason it can't.
    std::string CheckProjectName(const std::string& name);

    // Copies the template into `folder`, which must not exist or be empty, and writes
    // <name>.vkproject there. Nothing, logged, if it fails.
    std::optional<Project::Project> CreateProject(const ProjectTemplate& projectTemplate,
                                                  const std::filesystem::path& folder, const std::string& name);

} // namespace VKE::Editor
