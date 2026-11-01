#include <core/EntryPoint.h>
#include <core/Application.h>
#include <core/Globals.h>
#include <core/Log.h>
#include <core/System.h>
#include <assets/AssetManager.h>
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
        RuntimeSystem(const VKE::Project::Project& project, VKE::Assets::AssetManager& assets,
                      VKE::Renderer::Renderer& renderer)
            : m_StartScene(project.GetStartScene()), m_Library(project.GetGameLibrary()),
              m_WorkFolder(project.GetGameWorkFolder()), m_Assets(assets), m_Renderer(renderer) {
        }

        void OnInitialize() override {
            m_Game.Load(m_Library, m_WorkFolder);
            if (m_Assets.GetType(m_StartScene) != VKE::Assets::AssetType::Scene) {
                VKE_CORE_ERROR("Runtime: the project has no start scene");
                VKE::RequestExit();
                return;
            }
            VKE::Scene::PrefabLibrary prefabs(m_Assets);
            if (!VKE::Scene::SceneSerializer::Load(m_Scene, prefabs, m_Assets.GetFile(m_StartScene))) {
                VKE::RequestExit();
                return;
            }
            m_Runner.Start(m_Scene);
        }

        void OnUpdate(VKE::FrameTime ft) override {
            m_Runner.Update(ft.GetDeltaF());
            m_Renderer.Render(m_Scene.GetRenderScene(), m_Scene.GetView());
        }

        std::string_view GetName() const override { return "RuntimeSystem"; }

    private:
        VKE::Assets::AssetID m_StartScene;
        std::filesystem::path m_Library;
        std::filesystem::path m_WorkFolder;
        VKE::Assets::AssetManager& m_Assets;
        VKE::Renderer::Renderer& m_Renderer;
        // Before the scene, which holds objects of the game's types.
        VKE::Scripting::GameModule m_Game;
        VKE::Scene::Scene m_Scene;
        VKE::Scripting::ScriptRunner m_Runner;
    };

} // namespace

// Started with --project <folder>: runs that project's game, with no editor.
class RuntimeApp : public VKE::Application {
public:
    RuntimeApp() {
        std::filesystem::path folder =
            VKE::Project::FindProjectArgument(VKE::Platform::Process::GetCommandLineArguments());
        std::optional<VKE::Project::Project> project;
        if (!folder.empty()) project = VKE::Project::Project::Open(folder);
        if (!project) {
            VKE_CORE_ERROR("Runtime: start it with --project <folder>");
            VKE::RequestExit();
            return;
        }

        auto* platform =
            Register<VKE::Platform::PlatformSystem>(VKE::Platform::WindowDesc{project->GetName(), 1280, 720, true});
        auto* assets = Register<VKE::Assets::AssetManager>(project->GetAssetRoots());
        auto* renderer = Register<VKE::Renderer::Renderer>(platform->GetWindow(), *assets);
        VKE::Scene::ComponentRegistry::RegisterEngineComponents();
        Register<RuntimeSystem>(*project, *assets, *renderer);
    }
};

VKE::Application* VKE::CreateApplication() {
    return new RuntimeApp();
}
