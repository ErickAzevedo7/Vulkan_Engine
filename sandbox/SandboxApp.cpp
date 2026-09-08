#include <core/EntryPoint.h>
#include <core/Application.h>
#include <core/Log.h>
#include <platform/PlatformSystem.h>

// Phase 2 verification target: opens a window and runs the main loop.
// RHI/rendering (the triangle test) lands on top of this same app once
// VKE_RHI exists.
class SandboxApp : public VKE::Application {
public:
    SandboxApp() {
        Register<VKE::Platform::PlatformSystem>(
            VKE::Platform::WindowDesc{ "Vulkan Engine — Sandbox", 1280, 720, true });
    }
};

VKE::Application* VKE::CreateApplication() {
    return new SandboxApp();
}
