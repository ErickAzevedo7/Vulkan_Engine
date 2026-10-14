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

    // Sandbox-only scenes: fills render scenes with test content and flies a camera around them.
    // Hold the right mouse button to look, WASD/QE to move, Shift to go faster, Escape to quit.
    // Keys 1-3 switch the helmet between its own material and two test materials,
    // -/= change the exposure, T turns shadows on and off, and Tab switches between the
    // helmet and Sponza, which loads the first time it is shown. The average frame time is
    // logged every few seconds.
    class SandboxSceneSystem : public System {
    public:
        SandboxSceneSystem(Platform::IInput& input, Assets::AssetManager& assets, Renderer::Renderer& renderer);

        void OnInitialize() override;
        void OnUpdate(FrameTime ft) override;

        std::string_view GetName() const override { return "SandboxSceneSystem"; }

    private:
        void UpdateCamera(f32 deltaTime);
        void UpdateFrameTimeLog(f32 deltaTime);
        void CreateHelmetScene();
        void CreateFloor();
        void CreateSponzaScene();
        void SwitchScene();

        Platform::IInput& m_Input;
        Assets::AssetManager& m_Assets;
        Renderer::Renderer& m_Renderer;

        Renderer::RenderScene m_HelmetScene;
        Renderer::RenderScene m_SponzaScene;
        Renderer::RenderScene* m_ActiveScene = &m_HelmetScene;
        bool m_SponzaCreated = false;

        Renderer::Camera m_Camera;
        // Where the camera was in the scene that isn't showing.
        Renderer::Camera m_OtherCamera;

        Renderer::MeshInstanceID m_Helmet;
        Renderer::MeshInstanceID m_TemporaryHelmet;

        Handle<Assets::MaterialData> m_SmoothPlastic;
        Handle<Assets::MaterialData> m_RoughMetal;

        f32 m_Exposure = 1.0f;
        bool m_ShadowsEnabled = true;
        f32 m_FrameTimeSum = 0.0f;
        u32 m_FrameCount = 0;
        f32 m_LastMouseX = 0.0f;
        f32 m_LastMouseY = 0.0f;
        bool m_Looking = false;
    };

} // namespace VKE
