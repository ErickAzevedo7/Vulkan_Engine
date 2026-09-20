#include "SandboxSceneSystem.h"

#include <assets/AssetManager.h>
#include <core/Globals.h>
#include <core/Log.h>
#include <platform/Input.h>
#include <renderer/Renderer.h>

namespace VKE {

    SandboxSceneSystem::SandboxSceneSystem(Platform::IInput& input, Assets::AssetManager& assets,
                                           Renderer::Renderer& renderer)
        : m_Input(input), m_Assets(assets), m_Renderer(renderer) {
    }

    void SandboxSceneSystem::OnInitialize() {
        Handle<Assets::MeshData> helmet = m_Assets.LoadMesh("DamagedHelmet/glTF/DamagedHelmet.gltf");

        Renderer::MeshInstanceDesc desc;
        desc.Mesh = helmet;
        m_Helmet = m_Scene.CreateMesh(desc);

        // Removed after a few seconds to exercise destroying an entry while rendering.
        desc.Transform = glm::translate(Mat4(1.0f), Vec3(2.5f, 0.0f, 0.0f));
        m_TemporaryHelmet = m_Scene.CreateMesh(desc);

        Renderer::LightDesc sun;
        sun.Type = Renderer::LightType::Directional;
        sun.Direction = Vec3(-0.4f, -1.0f, -0.6f);
        sun.Color = Vec3(1.0f, 0.96f, 0.9f);
        sun.Intensity = 3.0f;
        m_Scene.CreateLight(sun);

        Renderer::LightDesc blueLight;
        blueLight.Type = Renderer::LightType::Point;
        blueLight.Position = Vec3(1.5f, 1.0f, 1.5f);
        blueLight.Color = Vec3(0.3f, 0.5f, 1.0f);
        blueLight.Intensity = 20.0f;
        blueLight.Range = 8.0f;
        m_Scene.CreateLight(blueLight);

        Renderer::LightDesc orangeLight = blueLight;
        orangeLight.Position = Vec3(-1.5f, 0.3f, 1.0f);
        orangeLight.Color = Vec3(1.0f, 0.55f, 0.2f);
        m_Scene.CreateLight(orangeLight);

        Assets::MaterialData plastic;
        plastic.Name = "SmoothPlastic";
        plastic.BaseColor = Vec4(0.8f, 0.1f, 0.1f, 1.0f);
        plastic.Metallic = 0.0f;
        plastic.Roughness = 0.2f;
        m_SmoothPlastic = m_Assets.AddMaterial(plastic);

        Assets::MaterialData metal;
        metal.Name = "RoughMetal";
        metal.BaseColor = Vec4(0.95f, 0.8f, 0.5f, 1.0f);
        metal.Metallic = 1.0f;
        metal.Roughness = 0.7f;
        m_RoughMetal = m_Assets.AddMaterial(metal);

        m_Camera.Position = Vec3(1.25f, 0.5f, 4.5f);
        m_Camera.LookAt(Vec3(1.25f, 0.0f, 0.0f));
    }

    void SandboxSceneSystem::OnUpdate(FrameTime ft) {
        if (m_Input.IsKeyPressed(Platform::KeyCode::Escape)) {
            RequestExit();
        }

        UpdateCamera(ft.GetDeltaF());

        if (m_Input.IsKeyPressed(Platform::KeyCode::D1)) m_Scene.SetMaterials(m_Helmet, {});
        if (m_Input.IsKeyPressed(Platform::KeyCode::D2)) m_Scene.SetMaterials(m_Helmet, {m_SmoothPlastic});
        if (m_Input.IsKeyPressed(Platform::KeyCode::D3)) m_Scene.SetMaterials(m_Helmet, {m_RoughMetal});

        if (m_TemporaryHelmet.IsValid() && ft.GetTotalF() > 3.0f) {
            m_Scene.DestroyMesh(m_TemporaryHelmet);
            m_TemporaryHelmet = {};
            VKE_CORE_INFO("SandboxSceneSystem: destroyed the second helmet");
        }

        m_Scene.SetTransform(m_Helmet, glm::rotate(Mat4(1.0f), ft.GetTotalF() * 0.3f, Vec3(0.0f, 1.0f, 0.0f)));

        m_Renderer.Render(m_Scene, m_Camera.GetView());
    }

    void SandboxSceneSystem::UpdateCamera(f32 deltaTime) {
        using Platform::KeyCode;

        f32 mouseX = m_Input.GetMouseX();
        f32 mouseY = m_Input.GetMouseY();

        if (m_Input.IsMouseButtonDown(Platform::MouseButton::Right)) {
            if (m_Looking) {
                constexpr f32 kSensitivity = 0.003f;
                constexpr f32 kPitchLimit = 1.55f;
                m_Camera.Yaw += (mouseX - m_LastMouseX) * kSensitivity;
                m_Camera.Pitch -= (mouseY - m_LastMouseY) * kSensitivity;
                m_Camera.Pitch = glm::clamp(m_Camera.Pitch, -kPitchLimit, kPitchLimit);
            }
            m_Looking = true;
        } else {
            m_Looking = false;
        }

        m_LastMouseX = mouseX;
        m_LastMouseY = mouseY;

        Vec3 forward = m_Camera.GetForward();
        Vec3 right = m_Camera.GetRight();
        Vec3 up(0.0f, 1.0f, 0.0f);

        Vec3 move(0.0f);
        if (m_Input.IsKeyDown(KeyCode::W)) move += forward;
        if (m_Input.IsKeyDown(KeyCode::S)) move -= forward;
        if (m_Input.IsKeyDown(KeyCode::D)) move += right;
        if (m_Input.IsKeyDown(KeyCode::A)) move -= right;
        if (m_Input.IsKeyDown(KeyCode::E)) move += up;
        if (m_Input.IsKeyDown(KeyCode::Q)) move -= up;

        if (glm::length(move) > 0.0f) {
            f32 speed = m_Input.IsKeyDown(KeyCode::LeftShift) ? 6.0f : 2.0f;
            m_Camera.Position += glm::normalize(move) * speed * deltaTime;
        }
    }

} // namespace VKE
