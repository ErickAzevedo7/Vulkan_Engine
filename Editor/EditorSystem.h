#pragma once

#include "EditorScene.h"
#include "MaterialEdits.h"
#include "Panels/AssetBrowserPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/GamePanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ProfilerPanel.h"
#include "Panels/ViewportPanel.h"
#include "ScriptCompiler.h"

#include <assets/AssetTypes.h>
#include <core/System.h>
#include <core/Types.h>
#include <project/Project.h>
#include <scripting/GameModule.h>

#include <string>

namespace VKE {
class PlatformSystem;
} // namespace VKE

namespace VKE {
class AssetManager;
} // namespace VKE

namespace VKE {
class Renderer;
} // namespace VKE

namespace VKE {
class UISystem;
} // namespace VKE

namespace VKE::Editor {

// The editor on an open project: the menu bar, the docked panels, the status bar and the
// scene being edited. It opens the scene last edited in the project, or else the project's
// start scene, and asks before anything would lose unsaved changes.
class EditorSystem : public System {
public:
	EditorSystem(Project project, PlatformSystem &platform, AssetManager &assets, Renderer &renderer, UISystem &ui);
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
	void Request(Action action, AssetID asset = {});
	// Whether `action` would lose changes: the scene's, and for closing, the materials'.
	bool HasUnsavedChanges(Action action) const;
	void Perform(Action action, AssetID asset);

	// Saves to the scene's file, or asks for one; false if it wasn't saved.
	bool SaveScene();
	bool SaveSceneAs();
	void ChooseSceneToOpen();
	bool OpenScene(AssetID scene);
	void SetStartScene();

	void HandleShortcuts();
	void DrawMenuBar();
	void DrawPlayBar();
	void StartPlay();
	void StopPlay();
	void DrawStatusBar();
	// The build of the scripts: what it says goes to the Console, and a new DLL gets loaded.
	void UpdateScripts();
	// What the status bar says about the scripts, with its color; empty for nothing.
	std::string GetScriptStatus(u32 &outColor) const;
	void DrawDockSpace();
	void DrawUnsavedDialog();
	void DrawRecoveryDialog();
	void DrawFaultDialog();
	void UpdateTitle();
	void UpdateFrameStats(f32 deltaTime);

	void LoadEditorState();
	void SaveEditorState() const;

	Project m_Project;
	PlatformSystem &m_Platform;
	AssetManager &m_Assets;
	Renderer &m_Renderer;
	UISystem &m_UI;

	ScriptCompiler m_ScriptCompiler;
	bool m_GameLoadFailed = false;
	u32 m_ScriptErrors = 0;
	// Before the scene, which holds objects of the game's types.
	GameModule m_GameModule;
	EditorScene m_Scene;
	MaterialEdits m_MaterialEdits;
	HierarchyPanel m_Hierarchy;
	InspectorPanel m_Inspector;
	ConsolePanel m_Console;
	ViewportPanel m_Viewport;
	AssetBrowserPanel m_AssetBrowser;
	GamePanel m_Game;
	ProfilerPanel m_Profiler;
	bool m_ShowHierarchy = true;
	bool m_ShowInspector = true;
	bool m_ShowViewport = true;
	bool m_ShowConsole = true;
	bool m_ShowAssets = true;
	bool m_ShowGame = true;
	bool m_ShowProfiler = false;
	// Files may change on disk while the editor is in the background: they are scanned
	// again when it gets the focus back.
	bool m_WindowFocused = true;
	bool m_ResetLayout = false;
	bool m_LayoutChecked = false;

	Action m_PendingAction = Action::None;
	AssetID m_PendingAsset;
	bool m_OpenUnsavedDialog = false;
	bool m_OpenRecoveryDialog = false;
	// What crashed in Play, while the dialog about it shows.
	std::string m_FaultMessage;
	bool m_OpenFaultDialog = false;
	// What goes on once the open prefab is closed.
	Action m_AfterPrefab = Action::None;
	AssetID m_AfterPrefabAsset;
	// Read from the editor's settings: the scene to open first.
	AssetID m_LastScene;

	std::string m_Title;
	f32 m_FrameTimeSum = 0.0f;
	u32 m_FrameCount = 0;
	f32 m_AverageFrameTime = 0.0f;
};

} // namespace VKE::Editor
