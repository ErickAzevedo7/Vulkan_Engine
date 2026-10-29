#pragma once

#include "EditorScene.h"
#include "Panels/ConsolePanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ViewportPanel.h"

#include <assets/AssetTypes.h>
#include <core/System.h>
#include <core/Types.h>
#include <project/Project.h>

#include <string>

namespace VKE::Platform {
    class PlatformSystem;
} // namespace VKE::Platform

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Renderer {
    class Renderer;
} // namespace VKE::Renderer

namespace VKE::UI {
    class UISystem;
} // namespace VKE::UI

namespace VKE::Editor {

    // The editor on an open project: the menu bar, the docked panels, the status bar and the
    // scene being edited. It opens the scene last edited in the project, or else the project's
    // start scene, and asks before anything would lose unsaved changes.
    class EditorSystem : public System {
    public:
        EditorSystem(Project::Project project, Platform::PlatformSystem& platform, Assets::AssetManager& assets,
                     Renderer::Renderer& renderer, UI::UISystem& ui);
        ~EditorSystem() override;

        void OnInitialize() override;
        void OnUpdate(FrameTime ft) override;
        void OnShutdown() override;

        std::string_view GetName() const override { return "EditorSystem"; }

    private:
        // What waits for an answer about unsaved changes.
        enum class Action {
            None,
            NewScene,
            OpenScene,
            CloseProject,
            Quit,
        };

        // Does `action`, asking first to save the scene if it has unsaved changes.
        void Request(Action action, Assets::AssetID scene = {});
        void Perform(Action action, Assets::AssetID scene);

        // Saves to the scene's file, or asks for one; false if it wasn't saved.
        bool SaveScene();
        bool SaveSceneAs();
        void ChooseSceneToOpen();
        bool OpenScene(Assets::AssetID scene);
        void SetStartScene();

        void HandleShortcuts();
        void DrawMenuBar();
        void DrawStatusBar();
        void DrawDockSpace();
        void DrawUnsavedDialog();
        void UpdateTitle();
        void UpdateFrameStats(f32 deltaTime);

        void LoadEditorState();
        void SaveEditorState() const;

        Project::Project m_Project;
        Platform::PlatformSystem& m_Platform;
        Assets::AssetManager& m_Assets;
        Renderer::Renderer& m_Renderer;
        UI::UISystem& m_UI;

        EditorScene m_Scene;
        HierarchyPanel m_Hierarchy;
        InspectorPanel m_Inspector;
        ConsolePanel m_Console;
        ViewportPanel m_Viewport;
        bool m_ShowHierarchy = true;
        bool m_ShowInspector = true;
        bool m_ShowViewport = true;
        bool m_ShowConsole = true;
        bool m_ResetLayout = false;
        bool m_LayoutChecked = false;

        Action m_PendingAction = Action::None;
        Assets::AssetID m_PendingScene;
        bool m_OpenUnsavedDialog = false;
        // Read from the editor's settings: the scene to open first.
        Assets::AssetID m_LastScene;

        std::string m_Title;
        f32 m_FrameTimeSum = 0.0f;
        u32 m_FrameCount = 0;
        f32 m_AverageFrameTime = 0.0f;
    };

} // namespace VKE::Editor
