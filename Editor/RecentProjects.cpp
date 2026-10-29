#include "RecentProjects.h"

#include <core/Log.h>
#include <platform/FileSystem.h>
#include <project/Project.h>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <fstream>

namespace VKE::Editor {

    namespace {
        constexpr size_t kMaxProjects = 20;

        std::filesystem::path GetListFile() {
            return Platform::FileSystem::GetUserDataDirectory() / "RecentProjects.yaml";
        }

        // Folders compare by their full path, ignoring case on Windows.
        std::string MakeKey(const std::filesystem::path& folder) {
            std::error_code ec;
            std::string key = std::filesystem::weakly_canonical(folder, ec).generic_u8string();
#ifdef _WIN32
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
            return key;
        }
    } // namespace

    void RecentProjects::Load() {
        m_Projects.clear();
        std::ifstream stream(GetListFile(), std::ios::binary);
        if (!stream) return;

        YAML::Node root;
        try {
            root = YAML::Load(stream);
        } catch (const YAML::Exception& error) {
            VKE_CORE_WARN("Editor: the recent projects list can't be read: {}", error.what());
            return;
        }
        YAML::Node projects = root["Projects"];
        if (!projects || !projects.IsSequence()) return;
        for (const YAML::Node& node : projects) {
            if (!node.IsMap() || !node["Folder"] || !node["Folder"].IsScalar()) continue;
            RecentProject project;
            project.Folder = std::filesystem::u8path(node["Folder"].Scalar());
            project.Name = node["Name"] && node["Name"].IsScalar() ? node["Name"].Scalar() : std::string();
            try {
                project.LastOpened = node["LastOpened"] ? node["LastOpened"].as<std::time_t>() : 0;
            } catch (const YAML::Exception&) {
            }
            m_Projects.push_back(std::move(project));
        }
        std::stable_sort(m_Projects.begin(), m_Projects.end(),
                         [](const RecentProject& a, const RecentProject& b) { return a.LastOpened > b.LastOpened; });
    }

    void RecentProjects::Save() const {
        YAML::Emitter out;
        out << YAML::Comment("Written by the VKE editor.");
        out << YAML::BeginMap << YAML::Key << "Projects" << YAML::Value << YAML::BeginSeq;
        for (const RecentProject& project : m_Projects) {
            out << YAML::BeginMap;
            out << YAML::Key << "Folder" << YAML::Value << YAML::DoubleQuoted << project.Folder.generic_u8string();
            out << YAML::Key << "Name" << YAML::Value << YAML::DoubleQuoted << project.Name;
            out << YAML::Key << "LastOpened" << YAML::Value << static_cast<long long>(project.LastOpened);
            out << YAML::EndMap;
        }
        out << YAML::EndSeq << YAML::EndMap;

        std::ofstream stream(GetListFile(), std::ios::binary | std::ios::trunc);
        stream << out.c_str() << '\n';
        if (!stream) VKE_CORE_WARN("Editor: couldn't save the recent projects list");
    }

    void RecentProjects::Add(const Project::Project& project) {
        Remove(project.GetFolder());
        m_Projects.insert(m_Projects.begin(), {project.GetFolder(), project.GetName(), std::time(nullptr)});
        if (m_Projects.size() > kMaxProjects) m_Projects.resize(kMaxProjects);
    }

    void RecentProjects::Remove(const std::filesystem::path& folder) {
        std::string key = MakeKey(folder);
        m_Projects.erase(std::remove_if(m_Projects.begin(), m_Projects.end(),
                                        [&](const RecentProject& project) { return MakeKey(project.Folder) == key; }),
                         m_Projects.end());
    }

} // namespace VKE::Editor
