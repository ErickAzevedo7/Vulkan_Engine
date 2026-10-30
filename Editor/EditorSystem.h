#pragma once

#include "EditorScene.h"
#include "MaterialEdits.h"
#include "Panels/AssetBrowserPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/GamePanel.h"
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
            OpenPrefab,
            ClosePrefab,
            CloseProject,
            Quit,
        };

        // Does `action`, asking first to save the scene if it has unsaved changes, and the
        // materials too when the project closes. `asset` is the scene or prefab to open. With a
        // prefab open, it is closed first, the same way.
        void Request(Action action, Assets::AssetID asset = {});
        // Whether `action` would lose changes: the scene's, and for closing, the materials'.
        bool HasUnsavedChanges(Action action) const;
        void Perform(Action action, Assets::AssetID asset);

        // Saves to the scene's file, or asks for one; false if it wasn't saved.
        bool SaveScene();
        bool SaveSceneAs();
        void ChooseSceneToOpen();
        bool OpenScene(Assets::AssetID scene);
        void SetStartScene();

        void HandleShortcuts();
        void DrawMenuBar();
        void DrawPlayBar();
        void StartPlay();
        void StopPlay();
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
        MaterialEdits m_MaterialEdits;
        HierarchyPanel m_Hierarchy;
        InspectorPanel m_Inspector;
        ConsolePanel m_Console;
        ViewportPanel m_Viewport;
        AssetBrowserPanel m_AssetBrowser;
        GamePanel m_Game;
        bool m_ShowHierarchy = true;
        bool m_ShowInspector = true;
        bool m_ShowViewport = true;
        bool m_ShowConsole = true;
        bool m_ShowAssets = true;
        bool m_ShowGame = true;
        // Files may change on disk while the editor is in the background: they are scanned
        // again when it gets the focus back.
        bool m_WindowFocused = true;
        bool m_ResetLayout = false;
        bool m_LayoutChecked = false;

        Action m_PendingAction = Action::None;
        Assets::AssetID m_PendingAsset;
        bool m_OpenUnsavedDialog = false;
        // What goes on once the open prefab is closed.
        Action m_AfterPrefab = Action::None;
        Assets::AssetID m_AfterPrefabAsset;
        // Read from the editor's settings: the scene to open first.
        Assets::AssetID m_LastScene;

        std::string m_Title;
        f32 m_FrameTimeSum = 0.0f;
        u32 m_FrameCount = 0;
        f32 m_AverageFrameTime = 0.0f;
    };

} // namespace VKE::Editor
