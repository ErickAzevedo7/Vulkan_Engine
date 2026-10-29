#include <core/EntryPoint.h>
#include <core/Application.h>
#include <core/Globals.h>
#include <assets/AssetManager.h>
#include <platform/PlatformSystem.h>
#include <platform/Process.h>
#include <project/Project.h>
#include <renderer/Renderer.h>
#include <scene/ComponentRegistry.h>
#include <ui/UISystem.h>

#include "SandboxSceneSystem.h"

// Opens a window and renders test scenes from the project given with --project <folder>.
class SandboxApp : public VKE::Application {
public:
    SandboxApp() {
        std::filesystem::path folder = VKE::Project::FindProjectArgument(VKE::Platform::Process::GetCommandLineArguments());
        std::optional<VKE::Project::Project> project;
        if (!folder.empty()) project = VKE::Project::Project::Open(folder);
        if (!project) {
            VKE_CORE_ERROR("Sandbox: start it with --project <folder>, a project with the sandbox's test assets");
            VKE::RequestExit();
            return;
        }

        auto* platform = Register<VKE::Platform::PlatformSystem>(
            VKE::Platform::WindowDesc{ "Vulkan Engine — Sandbox", 1280, 720, true });
        auto* assets = Register<VKE::Assets::AssetManager>(project->GetAssetRoots());
        auto* renderer = Register<VKE::Renderer::Renderer>(platform->GetWindow(), *assets);
        auto* ui = Register<VKE::UI::UISystem>(platform->GetWindow(), platform->GetInput(), *renderer);
        VKE::Scene::ComponentRegistry::RegisterEngineComponents();
        Register<VKE::SandboxSceneSystem>(platform->GetInput(), *assets, *renderer, *ui);
    }
};

VKE::Application* VKE::CreateApplication() {
    return new SandboxApp();
}
