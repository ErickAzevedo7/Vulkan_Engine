#pragma once

#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace VKE::Project {
    class Project;
} // namespace VKE::Project

namespace VKE::Editor {

    struct RecentProject {
        std::filesystem::path Folder;
        // As it was when last opened, for projects that have gone missing since.
        std::string Name;
        std::time_t LastOpened = 0;
    };

    // The projects opened lately, newest first, kept in RecentProjects.yaml in the user's data
    // folder so the project screen lists them on any run.
    class RecentProjects {
    public:
        void Load();
        void Save() const;

        // Puts the project first, opened now.
        void Add(const Project::Project& project);
        void Remove(const std::filesystem::path& folder);

        const std::vector<RecentProject>& Get() const { return m_Projects; }

    private:
        std::vector<RecentProject> m_Projects;
    };

} // namespace VKE::Editor
