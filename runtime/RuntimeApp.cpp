#include <assets/AssetManager.h>
#include <core/Application.h>
#include <core/EntryPoint.h>
#include <core/Globals.h>
#include <core/Log.h>
#include <core/System.h>
#include <platform/PlatformSystem.h>
#include <platform/Process.h>
#include <project/Project.h>
#include <renderer/Renderer.h>
#include <scene/ComponentRegistry.h>
#include <scene/Prefab.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scripting/GameModule.h>
#include <scripting/ScriptRunner.h>

namespace {

// Plays the project's start scene from its main camera until the window closes.
class RuntimeSystem : public VKE::System {
public:
	RuntimeSystem(
			const VKE::Project &project, VKE::AssetManager &assets, VKE::Renderer &renderer, const VKE::IInput &input) :
			m_StartScene(project.GetStartScene()),
			m_Library(project.GetGameLibrary()),
			m_WorkFolder(project.GetGameWorkFolder()),
			m_Assets(assets),
			m_Renderer(renderer),
			m_Input(input),
			m_Prefabs(assets) {}

	void OnInitialize() override {
		m_Game.Load(m_Library, m_WorkFolder);
		if (m_Assets.GetType(m_StartScene) != VKE::AssetType::Scene) {
			VKE_CORE_ERROR("Runtime: the project has no start scene");
			VKE::RequestExit();
			return;
		}
		if (!LoadScene(m_StartScene)) {
			VKE::RequestExit();
		}
	}

	void OnUpdate(VKE::FrameTime ft) override {
		m_Runner.Update(ft.GetDeltaF());
		m_Renderer.Render(m_Scene->GetRenderScene(), m_Scene->GetView());
	}

	std::string_view GetName() const override { return "RuntimeSystem"; }

private:
	// The current scene stays if the new one can't be read.
	bool LoadScene(VKE::AssetID scene) {
		auto loaded = VKE::MakeIunique<VKE::Scene>();
		if (m_Assets.GetType(scene) != VKE::AssetType::Scene ||
				!VKE::SceneSerializer::Load(*loaded, m_Prefabs, m_Assets.GetFile(scene))) {
			VKE_CORE_ERROR("Runtime: scene {} can't be loaded", scene.ToString());
			return false;
		}
		m_Runner.Stop();
		m_Scene = std::move(loaded);
		m_Runner.Start(*m_Scene,
				{ m_Prefabs, m_Input, [this](VKE::AssetID next) { LoadScene(next); }, [] { VKE::RequestExit(); },
						[](const std::string &) { VKE::RequestExit(); } });
		return true;
	}

	VKE::AssetID m_StartScene;
	std::filesystem::path m_Library;
	std::filesystem::path m_WorkFolder;
	VKE::AssetManager &m_Assets;
	VKE::Renderer &m_Renderer;
	const VKE::IInput &m_Input;
	// Before the prefabs and the scene, which hold objects of the game's types.
	VKE::GameModule m_Game;
	VKE::PrefabLibrary m_Prefabs;
	VKE::Iunique<VKE::Scene> m_Scene;
	VKE::ScriptRunner m_Runner;
};

} // namespace

// Started with --project <folder>: runs that project's game, with no editor.
class RuntimeApp : public VKE::Application {
public:
	RuntimeApp() {
		std::filesystem::path folder = VKE::FindProjectArgument(VKE::Process::GetCommandLineArguments());
		std::optional<VKE::Project> project;
		if (!folder.empty()) {
			project = VKE::Project::Open(folder);
		}
		if (!project) {
			VKE_CORE_ERROR("Runtime: start it with --project <folder>");
			VKE::RequestExit();
			return;
		}

		auto *platform = Register<VKE::PlatformSystem>(VKE::WindowDesc{ project->GetName(), 1280, 720, true });
		auto *assets = Register<VKE::AssetManager>(project->GetAssetRoots());
		auto *renderer = Register<VKE::Renderer>(platform->GetWindow(), *assets);
		VKE::ComponentRegistry::RegisterEngineComponents();
		Register<RuntimeSystem>(*project, *assets, *renderer, platform->GetInput());
	}
};

VKE::Application *VKE::CreateApplication() {
	return new RuntimeApp();
}
