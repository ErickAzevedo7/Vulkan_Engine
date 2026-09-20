#include <core/EntryPoint.h>
#include <core/Application.h>
#include <assets/AssetManager.h>
#include <platform/PlatformSystem.h>
#include <renderer/Renderer.h>

#include "SandboxSceneSystem.h"

// Opens a window and renders a test scene.
class SandboxApp : public VKE::Application {
public:
    SandboxApp() {
        auto* platform = Register<VKE::Platform::PlatformSystem>(
            VKE::Platform::WindowDesc{ "Vulkan Engine — Sandbox", 1280, 720, true });
        auto* assets = Register<VKE::Assets::AssetManager>(VKE_SANDBOX_ASSET_ROOT);
        auto* renderer = Register<VKE::Renderer::Renderer>(platform->GetWindow(), *assets);
        Register<VKE::SandboxSceneSystem>(platform->GetInput(), *assets, *renderer);
    }
};

VKE::Application* VKE::CreateApplication() {
    return new SandboxApp();
}
