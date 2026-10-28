#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/System.h>
#include <core/Types.h>
#include <math/MathTypes.h>
#include <renderer/Overlay.h>

#include <filesystem>
#include <string>

namespace VKE::Platform {
    class IInput;
} // namespace VKE::Platform

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Renderer {
    class Renderer;
} // namespace VKE::Renderer

namespace VKE::UI {
    class UISystem;
} // namespace VKE::UI

namespace VKE::Scene {
    class GameObject;
    struct LightSettings;
    class Scene;
} // namespace VKE::Scene

namespace VKE {

    // Sandbox-only scenes, built from GameObjects, with a flying camera in each.
    // Hold the right mouse button to look, WASD/QE to move, Shift to go faster, Escape to quit.
    // Keys 1-3 switch the helmet between its own material and two test materials,
    // -/= change the exposure, T turns shadows on and off, and Tab switches between the
    // helmet and Sponza, which loads the first time it is shown. In Sponza, the arrow keys
    // move a box along the floor and PageUp/PageDown raise and lower it; the walls stop it,
    // and it turns green inside a trigger zone further down the atrium. F5 saves the scene
    // showing to sandbox/<Helmet|Sponza>.scene under the asset root, and F9 replaces it with
    // what that file holds. The average frame time is logged every few seconds.
    // F1 shows and hides the UI test: a docked panel with the frame's numbers, text input, a
    // file dialog and the design system's fonts and icons, and a Viewport panel that shows
    // the scene through a view texture. Over the UI, the mouse and keys go to it.
    class SandboxSceneSystem : public System {
    public:
        SandboxSceneSystem(Platform::IInput& input, Assets::AssetManager& assets, Renderer::Renderer& renderer,
                           UI::UISystem& ui);
        ~SandboxSceneSystem() override;

        void OnInitialize() override;
        void OnUpdate(FrameTime ft) override;
        void OnShutdown() override;

        std::string_view GetName() const override { return "SandboxSceneSystem"; }

    private:
        // A scene and the camera flying around it. Yaw and pitch are in radians; 0 looks down -Z.
        struct SandboxScene {
            Iunique<Scene::Scene> Instance;
            Handle<Scene::GameObject> Camera;
            f32 Yaw = 0.0f;
            f32 Pitch = 0.0f;
        };

        void UpdateCamera(SandboxScene& scene, f32 deltaTime);
        void UpdateHelmet(f32 totalTime);
        void UpdateFrameTimeLog(f32 deltaTime);
        void CreateHelmetScene();
        void CreateFloor();
        void CreateSponzaScene();
        void CreateCollisionTest(Scene::Scene& scene);
        void UpdateBox(f32 deltaTime);
        void ReportContacts(const Scene::Scene& scene);
        std::filesystem::path GetSceneFile() const;
        void SaveActiveScene();
        void ReloadActiveScene();
        void SwitchScene();
        void BuildUI();

        void CreateCamera(SandboxScene& scene, const Vec3& position, const Vec3& target);
        void CreateLight(Scene::Scene& scene, const std::string& name, const Vec3& position, const Vec3& direction,
                         const Scene::LightSettings& settings);

        Platform::IInput& m_Input;
        Assets::AssetManager& m_Assets;
        Renderer::Renderer& m_Renderer;
        UI::UISystem& m_UI;

        SandboxScene m_HelmetScene;
        SandboxScene m_SponzaScene;
        SandboxScene* m_ActiveScene = nullptr;

        Handle<Scene::GameObject> m_Helmet;
        // The helmet's own rotation, which the spin is applied on top of.
        Quat m_HelmetRotation{1.0f, 0.0f, 0.0f, 0.0f};
        Handle<Scene::GameObject> m_TemporaryHelmet;

        Handle<Scene::GameObject> m_Box;
        Handle<Scene::GameObject> m_TriggerZone;
        Handle<Assets::ModelData> m_Cube;
        Handle<Assets::MaterialData> m_FloorMaterial;
        Handle<Assets::MaterialData> m_BoxMaterial;
        Handle<Assets::MaterialData> m_BoxInsideMaterial;

        Handle<Assets::MaterialData> m_SmoothPlastic;
        Handle<Assets::MaterialData> m_RoughMetal;

        f32 m_LastMouseX = 0.0f;
        f32 m_LastMouseY = 0.0f;
        bool m_Looking = false;

        f32 m_Exposure = 1.0f;
        bool m_ShadowsEnabled = true;
        f32 m_FrameTimeSum = 0.0f;
        u32 m_FrameCount = 0;
        f32 m_LastAverageFrameTime = 0.0f;

        bool m_ShowUI = true;
        bool m_ViewportVisible = false;
        bool m_LayoutChecked = false;
        Renderer::ViewTextureID m_ViewTexture;
        std::string m_Note;
        std::string m_ChosenFile;
    };

} // namespace VKE
