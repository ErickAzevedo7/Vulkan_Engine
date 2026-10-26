#include "SandboxSceneSystem.h"

#include <assets/AssetManager.h>
#include <core/Globals.h>
#include <core/Log.h>
#include <platform/Input.h>
#include <renderer/Renderer.h>
#include <scene/GameObject.h>
#include <scene/ModelInstantiation.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/components/CameraComponent.h>
#include <scene/components/ColliderComponent.h>
#include <scene/components/LightComponent.h>
#include <scene/components/MeshComponent.h>

#include <cmath>
#include <utility>

namespace VKE {

    namespace {
        const Vec3 kWorldUp(0.0f, 1.0f, 0.0f);

        // The rotation that points an object's forward axis (-Z) along `direction`.
        Quat RotationTowards(const Vec3& direction) {
            Vec3 forward = glm::normalize(direction);
            Vec3 up = std::abs(forward.y) < 0.999f ? kWorldUp : Vec3(0.0f, 0.0f, 1.0f);
            return glm::quatLookAt(forward, up);
        }

        Scene::GameObject* FindRoot(Scene::Scene& scene, const std::string& name) {
            for (Scene::GameObject* root : scene.GetRootObjects()) {
                if (root->GetName() == name) return root;
            }
            return nullptr;
        }

        // Yaw turns right as it grows, which is a negative turn around +Y.
        Quat RotationFromAngles(f32 yaw, f32 pitch) {
            return glm::angleAxis(-yaw, kWorldUp) * glm::angleAxis(pitch, Vec3(1.0f, 0.0f, 0.0f));
        }
    } // namespace

    SandboxSceneSystem::SandboxSceneSystem(Platform::IInput& input, Assets::AssetManager& assets,
                                           Renderer::Renderer& renderer)
        : m_Input(input), m_Assets(assets), m_Renderer(renderer) {
    }

    SandboxSceneSystem::~SandboxSceneSystem() = default;

    void SandboxSceneSystem::OnInitialize() {
        m_Cube = m_Assets.LoadModel("sandbox/Cube.obj");
        m_FloorMaterial = m_Assets.LoadMaterial("sandbox/Floor.mat");
        m_BoxMaterial = m_Assets.LoadMaterial("sandbox/Box.mat");
        m_BoxInsideMaterial = m_Assets.LoadMaterial("sandbox/BoxInside.mat");
        m_SmoothPlastic = m_Assets.LoadMaterial("sandbox/SmoothPlastic.mat");
        m_RoughMetal = m_Assets.LoadMaterial("sandbox/RoughMetal.mat");

        CreateHelmetScene();
        m_ActiveScene = &m_HelmetScene;
    }

    void SandboxSceneSystem::OnShutdown() {
        m_ActiveScene = nullptr;
        m_SponzaScene.Instance.reset();
        m_HelmetScene.Instance.reset();
    }

    void SandboxSceneSystem::CreateCamera(SandboxScene& scene, const Vec3& position, const Vec3& target) {
        Vec3 direction = glm::normalize(target - position);
        scene.Pitch = std::asin(glm::clamp(direction.y, -1.0f, 1.0f));
        scene.Yaw = std::atan2(direction.x, -direction.z);

        Scene::GameObject& camera = scene.Instance->CreateObject("Camera");
        camera.GetTransform().SetLocalPosition(position);
        camera.GetTransform().SetLocalRotation(RotationFromAngles(scene.Yaw, scene.Pitch));
        camera.AddComponent<Scene::CameraComponent>();
        scene.Camera = camera.GetHandle();
    }

    void SandboxSceneSystem::CreateLight(Scene::Scene& scene, const std::string& name, const Vec3& position,
                                         const Vec3& direction, const Scene::LightSettings& settings) {
        Scene::GameObject& light = scene.CreateObject(name);
        light.GetTransform().SetLocalPosition(position);
        light.GetTransform().SetLocalRotation(RotationTowards(direction));
        light.AddComponent<Scene::LightComponent>(settings);
    }

    void SandboxSceneSystem::CreateHelmetScene() {
        m_HelmetScene.Instance = MakeIunique<Scene::Scene>();
        Scene::Scene& scene = *m_HelmetScene.Instance;

        CreateCamera(m_HelmetScene, Vec3(1.25f, 0.5f, 4.5f), Vec3(1.25f, 0.0f, 0.0f));

        Handle<Assets::ModelData> helmetModel = m_Assets.LoadModel("DamagedHelmet/glTF/DamagedHelmet.gltf");
        if (Scene::GameObject* helmet = Scene::InstantiateModel(scene, m_Assets, helmetModel)) {
            m_Helmet = helmet->GetHandle();
            m_HelmetRotation = helmet->GetTransform().GetLocal().Rotation;
        }

        // Destroyed after a few seconds to exercise destroying an object while rendering.
        if (Scene::GameObject* helmet = Scene::InstantiateModel(scene, m_Assets, helmetModel)) {
            helmet->GetTransform().SetLocalPosition(Vec3(2.5f, 0.0f, 0.0f));
            m_TemporaryHelmet = helmet->GetHandle();
        }

        CreateFloor();

        Scene::LightSettings sun;
        sun.Type = Renderer::LightType::Directional;
        sun.Color = Vec3(1.0f, 0.96f, 0.9f);
        sun.Intensity = 3.0f;
        sun.CastsShadow = true;
        CreateLight(scene, "Sun", Vec3(0.0f), Vec3(-0.4f, -1.0f, -0.6f), sun);

        Scene::LightSettings spot;
        spot.Type = Renderer::LightType::Spot;
        spot.Color = Vec3(1.0f, 0.85f, 0.6f);
        spot.Intensity = 60.0f;
        spot.Range = 15.0f;
        spot.InnerConeAngle = glm::radians(15.0f);
        spot.OuterConeAngle = glm::radians(25.0f);
        spot.CastsShadow = true;
        CreateLight(scene, "Spot", Vec3(-2.5f, 2.5f, 2.0f), Vec3(2.5f, -3.5f, -2.0f), spot);

        Scene::LightSettings blueLight;
        blueLight.Type = Renderer::LightType::Point;
        blueLight.Color = Vec3(0.3f, 0.5f, 1.0f);
        blueLight.Intensity = 20.0f;
        blueLight.Range = 8.0f;
        blueLight.CastsShadow = true;
        CreateLight(scene, "Blue light", Vec3(1.5f, 1.0f, 1.5f), Vec3(0.0f, -1.0f, 0.0f), blueLight);

        Scene::LightSettings orangeLight = blueLight;
        orangeLight.Color = Vec3(1.0f, 0.55f, 0.2f);
        CreateLight(scene, "Orange light", Vec3(-1.5f, 0.3f, 1.0f), Vec3(0.0f, -1.0f, 0.0f), orangeLight);
    }

    void SandboxSceneSystem::CreateSponzaScene() {
        m_SponzaScene.Instance = MakeIunique<Scene::Scene>();
        Scene::Scene& scene = *m_SponzaScene.Instance;

        CreateCamera(m_SponzaScene, Vec3(-10.0f, 1.8f, -0.4f), Vec3(10.0f, 3.0f, -0.4f));
        Scene::GameObject* sponza =
            Scene::InstantiateModel(scene, m_Assets, m_Assets.LoadModel("sponza-atrium-3/source/glTF/Sponza.gltf"));

        // Every part of the building collides with its own triangles.
        std::vector<Scene::GameObject*> pending;
        if (sponza) pending.push_back(sponza);
        while (!pending.empty()) {
            Scene::GameObject* object = pending.back();
            pending.pop_back();
            if (auto* mesh = object->GetComponent<Scene::MeshComponent>()) {
                object->AddComponent<Scene::ColliderComponent>()->SetMesh(m_Assets, mesh->GetMesh());
            }
            pending.insert(pending.end(), object->GetChildren().begin(), object->GetChildren().end());
        }

        CreateCollisionTest(scene);

        Scene::LightSettings sun;
        sun.Type = Renderer::LightType::Directional;
        sun.Color = Vec3(1.0f, 0.95f, 0.85f);
        sun.Intensity = 5.0f;
        sun.CastsShadow = true;
        CreateLight(scene, "Sun", Vec3(0.0f), Vec3(-0.3f, -1.0f, 0.15f), sun);

        // A row of shadowed lamps down the middle of the atrium.
        const Vec3 colors[] = {Vec3(1.0f, 0.6f, 0.3f), Vec3(0.4f, 0.6f, 1.0f), Vec3(1.0f, 0.6f, 0.3f),
                               Vec3(0.4f, 0.6f, 1.0f)};
        for (u32 i = 0; i < 4; ++i) {
            Scene::LightSettings lamp;
            lamp.Type = Renderer::LightType::Point;
            lamp.Color = colors[i];
            lamp.Intensity = 30.0f;
            lamp.Range = 8.0f;
            lamp.CastsShadow = true;
            CreateLight(scene, "Lamp " + std::to_string(i + 1), Vec3(-7.5f + 5.0f * static_cast<f32>(i), 1.5f, -0.4f),
                        Vec3(0.0f, -1.0f, 0.0f), lamp);
        }
    }

    void SandboxSceneSystem::CreateCollisionTest(Scene::Scene& scene) {
        constexpr f32 kBoxSize = 0.5f;

        // Starts sunk into the floor; the first update pushes it up onto it.
        Scene::GameObject* box = Scene::InstantiateModel(scene, m_Assets, m_Cube);
        if (!box) return;
        box->SetName("Box");
        box->GetTransform().SetLocalPosition(Vec3(-7.0f, 0.1f, -0.4f));
        box->GetTransform().SetLocalScale(Vec3(kBoxSize));
        if (auto* mesh = box->GetComponentInChildren<Scene::MeshComponent>()) mesh->SetMaterials({m_BoxMaterial});
        // The unit cube, scaled with the object.
        auto* collider = box->AddComponent<Scene::ColliderComponent>();
        collider->SetBox(Vec3(0.0f), Vec3(0.5f));
        collider->SetMotion(Physics::BodyMotion::Movable);
        m_Box = box->GetHandle();

        Scene::GameObject& zone = scene.CreateObject("Trigger zone");
        zone.GetTransform().SetLocalPosition(Vec3(-3.0f, 0.5f, -0.4f));
        auto* trigger = zone.AddComponent<Scene::ColliderComponent>();
        trigger->SetBox(Vec3(0.0f), Vec3(0.75f, 0.5f, 1.5f));
        trigger->SetTrigger(true);
        m_TriggerZone = zone.GetHandle();
    }

    void SandboxSceneSystem::UpdateBox(f32 deltaTime) {
        using Platform::KeyCode;
        constexpr f32 kSpeed = 2.0f;

        if (m_ActiveScene != &m_SponzaScene) return;
        Scene::GameObject* box = m_SponzaScene.Instance->Get(m_Box);
        if (!box) return;

        // Along the atrium (+X) and across it (+Z), as seen from where the camera starts.
        Vec3 move(0.0f);
        if (m_Input.IsKeyDown(KeyCode::Up)) move.x += 1.0f;
        if (m_Input.IsKeyDown(KeyCode::Down)) move.x -= 1.0f;
        if (m_Input.IsKeyDown(KeyCode::Right)) move.z += 1.0f;
        if (m_Input.IsKeyDown(KeyCode::Left)) move.z -= 1.0f;
        if (m_Input.IsKeyDown(KeyCode::PageUp)) move.y += 1.0f;
        if (m_Input.IsKeyDown(KeyCode::PageDown)) move.y -= 1.0f;
        if (glm::length(move) == 0.0f) return;

        Scene::TransformComponent& transform = box->GetTransform();
        transform.SetWorldPosition(transform.GetWorld().Position + glm::normalize(move) * kSpeed * deltaTime);
    }

    void SandboxSceneSystem::ReportContacts(const Scene::Scene& scene) {
        for (const Scene::Contact& contact : scene.GetContacts()) {
            if (contact.Phase == Physics::ContactPhase::Stay) continue;
            if (contact.A != m_Box && contact.B != m_Box) continue;

            Handle<Scene::GameObject> other = contact.A == m_Box ? contact.B : contact.A;
            const Scene::GameObject* otherObject = scene.Get(other);
            bool begins = contact.Phase == Physics::ContactPhase::Begin;
            VKE_CORE_INFO("SandboxSceneSystem: box {} {} '{}'", begins ? "touches" : "leaves",
                          contact.IsTrigger ? "trigger" : "solid", otherObject ? otherObject->GetName() : "(destroyed)");

            if (other == m_TriggerZone) {
                Scene::GameObject* box = scene.Get(m_Box);
                if (auto* mesh = box ? box->GetComponentInChildren<Scene::MeshComponent>() : nullptr) {
                    mesh->SetMaterials({begins ? m_BoxInsideMaterial : m_BoxMaterial});
                }
            }
        }
    }

    void SandboxSceneSystem::SwitchScene() {
        if (m_ActiveScene == &m_HelmetScene) {
            if (!m_SponzaScene.Instance) CreateSponzaScene();
            m_ActiveScene = &m_SponzaScene;
        } else {
            m_ActiveScene = &m_HelmetScene;
        }
        m_FrameTimeSum = 0.0f;
        m_FrameCount = 0;
    }

    void SandboxSceneSystem::UpdateCamera(SandboxScene& scene, f32 deltaTime) {
        using Platform::KeyCode;

        Scene::GameObject* camera = scene.Instance->Get(scene.Camera);
        if (!camera) return;
        Scene::TransformComponent& transform = camera->GetTransform();

        f32 mouseX = m_Input.GetMouseX();
        f32 mouseY = m_Input.GetMouseY();

        if (m_Input.IsMouseButtonDown(Platform::MouseButton::Right)) {
            if (m_Looking) {
                constexpr f32 kSensitivity = 0.003f;
                constexpr f32 kPitchLimit = 1.55f;
                scene.Yaw += (mouseX - m_LastMouseX) * kSensitivity;
                scene.Pitch -= (mouseY - m_LastMouseY) * kSensitivity;
                scene.Pitch = glm::clamp(scene.Pitch, -kPitchLimit, kPitchLimit);
                transform.SetWorldRotation(RotationFromAngles(scene.Yaw, scene.Pitch));
            }
            m_Looking = true;
        } else {
            m_Looking = false;
        }

        m_LastMouseX = mouseX;
        m_LastMouseY = mouseY;

        Vec3 forward = transform.GetWorld().GetForward();
        Vec3 right = transform.GetWorld().GetRight();

        Vec3 move(0.0f);
        if (m_Input.IsKeyDown(KeyCode::W)) move += forward;
        if (m_Input.IsKeyDown(KeyCode::S)) move -= forward;
        if (m_Input.IsKeyDown(KeyCode::D)) move += right;
        if (m_Input.IsKeyDown(KeyCode::A)) move -= right;
        if (m_Input.IsKeyDown(KeyCode::E)) move += kWorldUp;
        if (m_Input.IsKeyDown(KeyCode::Q)) move -= kWorldUp;

        if (glm::length(move) > 0.0f) {
            f32 speed = m_Input.IsKeyDown(KeyCode::LeftShift) ? 6.0f : 2.0f;
            transform.SetWorldPosition(transform.GetWorld().Position + glm::normalize(move) * speed * deltaTime);
        }
    }

    void SandboxSceneSystem::UpdateHelmet(f32 totalTime) {
        constexpr f32 kSpinSpeed = 0.3f;

        Scene::Scene& scene = *m_HelmetScene.Instance;
        Scene::GameObject* helmet = scene.Get(m_Helmet);
        if (!helmet) return;

        helmet->GetTransform().SetLocalRotation(glm::angleAxis(totalTime * kSpinSpeed, kWorldUp) * m_HelmetRotation);

        if (auto* mesh = helmet->GetComponentInChildren<Scene::MeshComponent>()) {
            if (m_Input.IsKeyPressed(Platform::KeyCode::D1)) mesh->SetMaterials({});
            if (m_Input.IsKeyPressed(Platform::KeyCode::D2)) mesh->SetMaterials({m_SmoothPlastic});
            if (m_Input.IsKeyPressed(Platform::KeyCode::D3)) mesh->SetMaterials({m_RoughMetal});
        }

        if (Scene::GameObject* temporaryHelmet = scene.Get(m_TemporaryHelmet); temporaryHelmet && totalTime > 3.0f) {
            scene.Destroy(*temporaryHelmet);
            VKE_CORE_INFO("SandboxSceneSystem: destroyed the second helmet");
        }
    }

    void SandboxSceneSystem::UpdateFrameTimeLog(f32 deltaTime) {
        constexpr f32 kLogInterval = 5.0f;

        m_FrameTimeSum += deltaTime;
        ++m_FrameCount;
        if (m_FrameTimeSum >= kLogInterval) {
            VKE_CORE_INFO("SandboxSceneSystem: {:.2f} ms per frame in the {} scene",
                          1000.0f * m_FrameTimeSum / static_cast<f32>(m_FrameCount),
                          m_ActiveScene == &m_HelmetScene ? "helmet" : "Sponza");
            m_FrameTimeSum = 0.0f;
            m_FrameCount = 0;
        }
    }

    void SandboxSceneSystem::OnUpdate(FrameTime ft) {
        if (m_Input.IsKeyPressed(Platform::KeyCode::Escape)) {
            RequestExit();
        }

        if (m_Input.IsKeyPressed(Platform::KeyCode::Tab)) {
            SwitchScene();
        }
        if (m_Input.IsKeyPressed(Platform::KeyCode::F5)) SaveActiveScene();
        if (m_Input.IsKeyPressed(Platform::KeyCode::F9)) ReloadActiveScene();

        UpdateFrameTimeLog(ft.GetDeltaF());

        f32 exposureStep = 0.0f;
        if (m_Input.IsKeyDown(Platform::KeyCode::Minus)) exposureStep -= 1.0f;
        if (m_Input.IsKeyDown(Platform::KeyCode::Equal)) exposureStep += 1.0f;
        if (exposureStep != 0.0f) {
            m_Exposure = glm::clamp(m_Exposure + exposureStep * ft.GetDeltaF(), 0.05f, 8.0f);
            m_Renderer.SetExposure(m_Exposure);
        }

        if (m_Input.IsKeyPressed(Platform::KeyCode::T)) {
            m_ShadowsEnabled = !m_ShadowsEnabled;
            m_Renderer.SetShadowsEnabled(m_ShadowsEnabled);
            VKE_CORE_INFO("SandboxSceneSystem: shadows {}", m_ShadowsEnabled ? "on" : "off");
        }

        UpdateCamera(*m_ActiveScene, ft.GetDeltaF());
        UpdateHelmet(ft.GetTotalF());
        UpdateBox(ft.GetDeltaF());

        Scene::Scene& scene = *m_ActiveScene->Instance;
        scene.Update();
        ReportContacts(scene);
        m_Renderer.Render(scene.GetRenderScene(), scene.GetView());
    }

    void SandboxSceneSystem::CreateFloor() {
        constexpr f32 kSize = 20.0f;
        constexpr f32 kThickness = 0.05f;
        constexpr f32 kHeight = -1.2f;

        Scene::GameObject* floor = Scene::InstantiateModel(*m_HelmetScene.Instance, m_Assets, m_Cube);
        if (!floor) return;
        floor->SetName("Floor");
        // The top face at kHeight.
        floor->GetTransform().SetLocalPosition(Vec3(0.0f, kHeight - 0.5f * kThickness, 0.0f));
        floor->GetTransform().SetLocalScale(Vec3(kSize, kThickness, kSize));
        if (auto* mesh = floor->GetComponentInChildren<Scene::MeshComponent>()) mesh->SetMaterials({m_FloorMaterial});
    }

    std::filesystem::path SandboxSceneSystem::GetSceneFile() const {
        const char* name = m_ActiveScene == &m_HelmetScene ? "Helmet.scene" : "Sponza.scene";
        return m_Assets.GetAssetRoot() / "sandbox" / name;
    }

    void SandboxSceneSystem::SaveActiveScene() {
        Scene::SceneSerializer::Save(*m_ActiveScene->Instance, m_Assets, GetSceneFile());
    }

    void SandboxSceneSystem::ReloadActiveScene() {
        auto scene = MakeIunique<Scene::Scene>();
        if (!Scene::SceneSerializer::Load(*scene, m_Assets, GetSceneFile())) return;

        SandboxScene& active = *m_ActiveScene;
        active.Instance = std::move(scene);

        // The handles the sandbox keeps point into the old scene; find the same objects again.
        active.Camera = {};
        if (Scene::CameraComponent* camera = active.Instance->GetMainCamera()) {
            active.Camera = camera->GetOwner().GetHandle();
            Vec3 forward = camera->GetTransform().GetWorld().GetForward();
            active.Pitch = std::asin(glm::clamp(forward.y, -1.0f, 1.0f));
            active.Yaw = std::atan2(forward.x, -forward.z);
        }
        auto find = [&](const char* name) {
            Scene::GameObject* object = FindRoot(*active.Instance, name);
            return object ? object->GetHandle() : Handle<Scene::GameObject>{};
        };
        if (m_ActiveScene == &m_HelmetScene) {
            m_Helmet = find("DamagedHelmet");
            m_TemporaryHelmet = {};
        } else {
            m_Box = find("Box");
            m_TriggerZone = find("Trigger zone");
        }
    }

} // namespace VKE
