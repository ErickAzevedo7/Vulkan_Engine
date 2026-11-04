#include "EditorSystem.h"
#include "LogCapture.h"
#include "ProjectHub.h"

#include <assets/AssetManager.h>
#include <core/Application.h>
#include <core/EntryPoint.h>
#include <platform/FileSystem.h>
#include <platform/PlatformSystem.h>
#include <platform/Process.h>
#include <project/Project.h>
#include <renderer/Renderer.h>
#include <scene/ComponentRegistry.h>
#include <ui/UISystem.h>

// Started with --project <folder>, the editor on that project; without it, or when the
// project can't be opened, the project screen.
class EditorApp : public VKE::Application {
public:
	EditorApp() {
		// First, so the Console has every message from here on.
		VKE::Editor::LogCapture::Install();

		std::filesystem::path folder = VKE::FindProjectArgument(VKE::Process::GetCommandLineArguments());
		std::optional<VKE::Project> project;
		std::string notice;
		if (!folder.empty()) {
			project = VKE::Project::Open(folder);
			if (!project) {
				notice = "'" + folder.u8string() + "' couldn't be opened as a project.";
			}
		}

		if (project) {
			StartEditor(std::move(*project));
		} else {
			StartProjectHub(std::move(notice));
		}
	}

private:
	void StartEditor(VKE::Project project) {
		auto *platform = Register<VKE::PlatformSystem>(VKE::WindowDesc{ "VKE Editor", 1600, 900, true });
		auto *assets = Register<VKE::AssetManager>(project.GetAssetRoots());
		auto *renderer = Register<VKE::Renderer>(platform->GetWindow(), *assets);
		auto *ui = Register<VKE::UISystem>(platform->GetWindow(), platform->GetInput(), *renderer);
		VKE::ComponentRegistry::RegisterEngineComponents();
		Register<VKE::Editor::EditorSystem>(std::move(project), *platform, *assets, *renderer, *ui);
	}

	void StartProjectHub(std::string notice) {
		auto *platform = Register<VKE::PlatformSystem>(VKE::WindowDesc{ "VKE Projects", 1120, 720, true });
		// Only the engine's content: the renderer needs an asset manager, and there is no project yet.
		auto *assets = Register<VKE::AssetManager>(
				std::vector<VKE::AssetRoot>{ { "Engine", VKE::FileSystem::GetEngineContentDirectory(), true } });
		auto *renderer = Register<VKE::Renderer>(platform->GetWindow(), *assets);
		auto *ui = Register<VKE::UISystem>(platform->GetWindow(), platform->GetInput(), *renderer);
		Register<VKE::Editor::ProjectHub>(platform->GetInput(), *renderer, *ui, std::move(notice));
	}
};

VKE::Application *VKE::CreateApplication() {
	return new EditorApp();
}
