#include <core/EntryPoint.h>
#include <core/Application.h>
#include <core/Log.h>
#include <platform/PlatformSystem.h>

#include "TriangleRenderSystem.h"

// Opens a window and draws a hardcoded triangle through the RHI, to verify
// the engine/window/graphics pipeline end-to-end.
class SandboxApp : public VKE::Application {
public:
    SandboxApp() {
        auto* platform = Register<VKE::Platform::PlatformSystem>(
            VKE::Platform::WindowDesc{ "Vulkan Engine — Sandbox", 1280, 720, true });
        Register<VKE::TriangleRenderSystem>(platform->GetWindow());
    }
};

VKE::Application* VKE::CreateApplication() {
    return new SandboxApp();
}
