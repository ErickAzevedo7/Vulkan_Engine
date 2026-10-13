#pragma once

#include <core/System.h>
#include <core/Types.h>
#include <renderer/Camera.h>
#include <renderer/RenderScene.h>

namespace VKE::Platform {
    class IInput;
} // namespace VKE::Platform

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Renderer {
    class Renderer;
} // namespace VKE::Renderer

namespace VKE {

    // Sandbox-only scene: fills a render scene with test content and flies a camera around it.
    // Hold the right mouse button to look, WASD/QE to move, Shift to go faster, Escape to quit.
    // Keys 1-3 switch the helmet between its own material and two test materials,
    // and -/= change the exposure.
    class SandboxSceneSystem : public System {
    public:
        SandboxSceneSystem(Platform::IInput& input, Assets::AssetManager& assets, Renderer::Renderer& renderer);

        void OnInitialize() override;
        void OnUpdate(FrameTime ft) override;

        std::string_view GetName() const override { return "SandboxSceneSystem"; }

    private:
        void UpdateCamera(f32 deltaTime);

        Platform::IInput& m_Input;
        Assets::AssetManager& m_Assets;
        Renderer::Renderer& m_Renderer;

        Renderer::RenderScene m_Scene;
        Renderer::Camera m_Camera;

        Renderer::MeshInstanceID m_Helmet;
        Renderer::MeshInstanceID m_TemporaryHelmet;

        Handle<Assets::MaterialData> m_SmoothPlastic;
        Handle<Assets::MaterialData> m_RoughMetal;

        f32 m_Exposure = 1.0f;
        f32 m_LastMouseX = 0.0f;
        f32 m_LastMouseY = 0.0f;
        bool m_Looking = false;
    };

} // namespace VKE
