#pragma once

#include "ProjectTemplates.h"
#include "RecentProjects.h"

#include <core/System.h>
#include <core/Types.h>

#include <filesystem>
#include <string>
#include <vector>

namespace VKE::Platform {
    class IInput;
} // namespace VKE::Platform

namespace VKE::Renderer {
    class Renderer;
} // namespace VKE::Renderer

namespace VKE::UI {
    class UISystem;
} // namespace VKE::UI

namespace VKE::Editor {

    // The screen the editor shows when started without a project: the recent projects, and
    // buttons to create one from a template or open a folder. Opening a project starts the
    // editor again with --project <folder> and closes this one.
    class ProjectHub : public System {
    public:
        // `notice` shows at the bottom until the next action, e.g. why a project didn't open.
        ProjectHub(Platform::IInput& input, Renderer::Renderer& renderer, UI::UISystem& ui, std::string notice);

        void OnInitialize() override;
        void OnUpdate(FrameTime ft) override;

        std::string_view GetName() const override { return "ProjectHub"; }

    private:
        void Draw();
        void DrawSidebar();
        void DrawRecentProjects();
        void DrawProjectRow(size_t index, f32 width);
        void DrawNewProjectDialog();
        void DrawTemplateCard(size_t index);

        void RefreshMissing();
        void BeginNewProject();
        void CreateNewProject();
        void OpenFolder();
        // Starts the editor on `folder` and closes the project screen. False, with a notice,
        // if the folder holds no project.
        bool OpenProject(const std::filesystem::path& folder);
        void SetNotice(std::string notice, bool isError);

        // The folder a new project would be created in, and why it can't be, if it can't.
        std::filesystem::path GetNewProjectFolder() const;
        std::string CheckNewProject() const;

        Platform::IInput& m_Input;
        Renderer::Renderer& m_Renderer;
        UI::UISystem& m_UI;

        RecentProjects m_Recent;
        // Whether each recent project's folder or .vkproject is gone.
        std::vector<bool> m_Missing;
        std::vector<ProjectTemplate> m_Templates;
        // Index into m_Recent; -1 for none.
        i64 m_Selected = -1;
        std::string m_Search;
        std::string m_Notice;
        bool m_NoticeIsError = false;

        bool m_OpenNewProjectDialog = false;
        size_t m_Template = 0;
        std::string m_NewName;
        std::string m_NewLocation;
        std::string m_CreateError;
    };

} // namespace VKE::Editor
