#include <core/EntryPoint.h>
#include <core/Application.h>
#include <assets/AssetManager.h>
#include <platform/FileSystem.h>
#include <platform/PlatformSystem.h>
#include <platform/Process.h>
#include <project/Project.h>
#include <renderer/Renderer.h>
#include <scene/ComponentRegistry.h>
#include <ui/UISystem.h>

#include "EditorSystem.h"
#include "LogCapture.h"
#include "ProjectHub.h"

// Started with --project <folder>, the editor on that project; without it, or when the
// project can't be opened, the project screen.
class EditorApp : public VKE::Application {
public:
    EditorApp() {
        // First, so the Console has every message from here on.
        VKE::Editor::LogCapture::Install();

        std::filesystem::path folder = VKE::Project::FindProjectArgument(VKE::Platform::Process::GetCommandLineArguments());
        std::optional<VKE::Project::Project> project;
        std::string notice;
        if (!folder.empty()) {
            project = VKE::Project::Project::Open(folder);
            if (!project) notice = "'" + folder.u8string() + "' couldn't be opened as a project.";
        }

        if (project) {
            StartEditor(std::move(*project));
        } else {
            StartProjectHub(std::move(notice));
        }
    }

private:
    void StartEditor(VKE::Project::Project project) {
        auto* platform = Register<VKE::Platform::PlatformSystem>(VKE::Platform::WindowDesc{"VKE Editor", 1600, 900, true});
        auto* assets = Register<VKE::Assets::AssetManager>(project.GetAssetRoots());
        auto* renderer = Register<VKE::Renderer::Renderer>(platform->GetWindow(), *assets);
        auto* ui = Register<VKE::UI::UISystem>(platform->GetWindow(), platform->GetInput(), *renderer);
        VKE::Scene::ComponentRegistry::RegisterEngineComponents();
        Register<VKE::Editor::EditorSystem>(std::move(project), *platform, *assets, *renderer, *ui);
    }

    void StartProjectHub(std::string notice) {
        auto* platform = Register<VKE::Platform::PlatformSystem>(VKE::Platform::WindowDesc{"VKE Projects", 1120, 720, true});
        // Only the engine's content: the renderer needs an asset manager, and there is no project yet.
        auto* assets = Register<VKE::Assets::AssetManager>(std::vector<VKE::Assets::AssetRoot>{
            {"Engine", VKE::Platform::FileSystem::GetEngineContentDirectory(), true}});
        auto* renderer = Register<VKE::Renderer::Renderer>(platform->GetWindow(), *assets);
        auto* ui = Register<VKE::UI::UISystem>(platform->GetWindow(), platform->GetInput(), *renderer);
        Register<VKE::Editor::ProjectHub>(platform->GetInput(), *renderer, *ui, std::move(notice));
    }
};

VKE::Application* VKE::CreateApplication() {
    return new EditorApp();
}
