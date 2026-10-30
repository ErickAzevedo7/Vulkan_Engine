#include "EditorScene.h"

#include <assets/AssetManager.h>
#include <core/Log.h>
#include <platform/FileSystem.h>
#include <scene/GameObject.h>
#include <scene/ModelInstantiation.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/components/CameraComponent.h>
#include <scene/components/LightComponent.h>
#include <scene/components/MeshComponent.h>

#include <cmath>

namespace VKE::Editor {

    namespace {
        const Vec3 kWorldUp(0.0f, 1.0f, 0.0f);

        // The rotation that points an object's forward axis (-Z) along `direction`.
        Quat RotationTowards(const Vec3& direction) {
            Vec3 forward = glm::normalize(direction);
            Vec3 up = std::abs(forward.y) < 0.999f ? kWorldUp : Vec3(0.0f, 0.0f, 1.0f);
            return glm::quatLookAt(forward, up);
        }

        const char* GetShapeName(Shape shape) {
            switch (shape) {
            case Shape::Cube: return "Cube";
            case Shape::Sphere: return "Sphere";
            case Shape::Plane: return "Plane";
            case Shape::Cylinder: return "Cylinder";
            }
            return "";
        }

        bool IsInside(const std::filesystem::path& file, const std::filesystem::path& folder) {
            std::error_code ec;
            std::string relative = std::filesystem::weakly_canonical(file, ec)
                                       .lexically_relative(std::filesystem::weakly_canonical(folder, ec))
                                       .generic_string();
            return !ec && !relative.empty() && relative != "." && relative.rfind("..", 0) != 0;
        }
    } // namespace

    EditorScene::EditorScene(Assets::AssetManager& assets) : m_Assets(assets), m_Scene(MakeIunique<Scene::Scene>()) {
    }

    EditorScene::~EditorScene() = default;

    void EditorScene::New() {
        m_Scene = MakeIunique<Scene::Scene>();
        m_ID = {};
        ++m_OpenCount;
        m_PlayMode = PlayMode::Edit;
        m_Dirty = false;
        m_Selected = {};

        Scene::GameObject& camera = m_Scene->CreateObject("Main camera");
        Vec3 position(0.0f, 1.5f, 5.0f);
        camera.GetTransform().SetLocalPosition(position);
        camera.GetTransform().SetLocalRotation(RotationTowards(Vec3(0.0f, 0.5f, 0.0f) - position));
        camera.AddComponent<Scene::CameraComponent>();

        Scene::LightSettings sun;
        sun.Color = Vec3(1.0f, 0.96f, 0.9f);
        sun.Intensity = 3.0f;
        sun.CastsShadow = true;
        Scene::GameObject& light = m_Scene->CreateObject("Sun");
        light.GetTransform().SetLocalRotation(RotationTowards(Vec3(-0.4f, -1.0f, -0.6f)));
        light.AddComponent<Scene::LightComponent>(sun);
        VKE_CORE_INFO("Editor: new scene");
    }

    bool EditorScene::Open(Assets::AssetID scene) {
        if (m_Assets.GetType(scene) != Assets::AssetType::Scene) {
            VKE_CORE_ERROR("Editor: {} isn't a scene", scene.ToString());
            return false;
        }
        std::filesystem::path file = m_Assets.GetFile(scene);
        auto loaded = MakeIunique<Scene::Scene>();
        if (!Scene::SceneSerializer::Load(*loaded, m_Assets, file)) return false;

        m_Scene = std::move(loaded);
        m_ID = scene;
        ++m_OpenCount;
        m_PlayMode = PlayMode::Edit;
        m_Dirty = false;
        m_Selected = {};
        return true;
    }

    bool EditorScene::Save() {
        std::filesystem::path file = GetFile();
        if (file.empty()) return false;
        if (!Scene::SceneSerializer::Save(*m_Scene, m_Assets, file)) return false;
        m_Dirty = false;
        return true;
    }

    bool EditorScene::SaveAs(const std::filesystem::path& file) {
        const Assets::AssetRoot& root = m_Assets.GetRoots().front();
        if (root.ReadOnly || !IsInside(file, root.Folder)) {
            VKE_CORE_ERROR("Editor: scenes are saved inside the project's assets folder, '{}'", root.Folder.u8string());
            return false;
        }
        if (!Scene::SceneSerializer::Save(*m_Scene, m_Assets, file)) return false;

        // The file is an asset now; saving over a scene keeps its ID.
        Assets::AssetID id = m_Assets.FindAsset(file.u8string());
        if (!id.IsValid()) return false;
        m_ID = id;
        m_Dirty = false;
        return true;
    }

    bool EditorScene::Play() {
        if (IsPlaying()) return true;
        m_PlaySnapshot = Scene::SceneSerializer::ToText(*m_Scene, m_Assets);
        if (m_PlaySnapshot.empty()) return false;
        m_DirtyAtPlay = m_Dirty;
        m_PlayMode = PlayMode::Playing;
        VKE_CORE_INFO("Editor: Play: scene kept in memory ({} objects); Stop restores it", m_Scene->GetObjectCount());
        return true;
    }

    void EditorScene::TogglePause() {
        if (!IsPlaying()) return;
        m_PlayMode = m_PlayMode == PlayMode::Playing ? PlayMode::Paused : PlayMode::Playing;
    }

    void EditorScene::Stop() {
        if (!IsPlaying()) return;
        Scene::GameObject* selected = GetSelected();
        ObjectID selectedID = selected ? selected->GetID() : ObjectID();

        auto restored = MakeIunique<Scene::Scene>();
        if (!Scene::SceneSerializer::LoadText(*restored, m_Assets, m_PlaySnapshot, "the scene kept at Play")) {
            VKE_CORE_ERROR("Editor: the scene kept at Play can't be restored; what is open stays");
            return;
        }
        m_Scene = std::move(restored);
        m_PlaySnapshot.clear();
        m_PlayMode = PlayMode::Edit;
        m_Dirty = m_DirtyAtPlay;
        Scene::GameObject* same = selectedID.IsValid() ? m_Scene->FindByID(selectedID) : nullptr;
        m_Selected = same ? same->GetHandle() : Handle<Scene::GameObject>();
        VKE_CORE_INFO("Editor: Stop: scene restored");
    }

    void EditorScene::Update() {
        if (m_PlayMode == PlayMode::Playing) {
            m_Scene->Update();
        } else {
            m_Scene->Sync();
        }
    }

    std::filesystem::path EditorScene::GetFile() const {
        return m_ID.IsValid() ? m_Assets.GetFile(m_ID) : std::filesystem::path();
    }

    std::string EditorScene::GetFileName() const {
        std::filesystem::path file = GetFile();
        return file.empty() ? std::string("Untitled") : file.filename().u8string();
    }

    Scene::GameObject* EditorScene::GetSelected() const {
        return m_Scene->Get(m_Selected);
    }

    void EditorScene::Select(Scene::GameObject* object) {
        m_Selected = object ? object->GetHandle() : Handle<Scene::GameObject>();
        m_SelectedAsset = {};
    }

    void EditorScene::SelectAsset(Assets::AssetID asset) {
        m_Selected = {};
        m_SelectedAsset = asset;
    }

    Scene::GameObject& EditorScene::CreateEmpty(Scene::GameObject* parent) {
        m_Dirty = true;
        return m_Scene->CreateObject("GameObject", parent);
    }

    Scene::GameObject* EditorScene::CreateShape(Shape shape, Scene::GameObject* parent) {
        const char* name = GetShapeName(shape);
        std::filesystem::path content = Platform::FileSystem::GetEngineContentDirectory();
        Handle<Assets::ModelData> model =
            m_Assets.LoadModel((content / "Shapes" / (std::string(name) + ".obj")).u8string());
        const Assets::ModelData* data = m_Assets.GetModel(model);
        Handle<Assets::MeshData> mesh;
        for (size_t i = 0; data && i < data->Nodes.size() && !mesh.IsValid(); ++i) {
            mesh = data->Nodes[i].Mesh;
        }
        if (!mesh.IsValid()) {
            VKE_CORE_ERROR("Editor: the engine's content has no {} shape", name);
            return nullptr;
        }

        Scene::GameObject& object = m_Scene->CreateObject(name, parent);
        auto* component = object.AddComponent<Scene::MeshComponent>(mesh);
        Handle<Assets::MaterialData> material = m_Assets.LoadMaterial((content / "Materials" / "Default.mat").u8string());
        if (material.IsValid()) component->SetMaterials({material});
        m_Dirty = true;
        return &object;
    }

    Scene::GameObject& EditorScene::CreateCamera(Scene::GameObject* parent) {
        Scene::GameObject& camera = m_Scene->CreateObject("Camera", parent);
        camera.GetTransform().SetLocalPosition(Vec3(0.0f, 1.0f, 5.0f));
        camera.AddComponent<Scene::CameraComponent>();
        m_Dirty = true;
        return camera;
    }

    Scene::GameObject& EditorScene::CreateLight(Renderer::LightType type, Scene::GameObject* parent) {
        Scene::LightSettings settings;
        settings.Type = type;
        const char* name = "Directional light";
        Vec3 position(0.0f);
        Vec3 direction(-0.4f, -1.0f, -0.6f);
        if (type == Renderer::LightType::Point) {
            name = "Point light";
            position = Vec3(0.0f, 2.0f, 0.0f);
            settings.Intensity = 20.0f;
        } else if (type == Renderer::LightType::Spot) {
            name = "Spot light";
            position = Vec3(0.0f, 3.0f, 0.0f);
            direction = Vec3(0.0f, -1.0f, 0.0f);
            settings.Intensity = 40.0f;
        }

        Scene::GameObject& light = m_Scene->CreateObject(name, parent);
        light.GetTransform().SetLocalPosition(position);
        light.GetTransform().SetLocalRotation(RotationTowards(direction));
        light.AddComponent<Scene::LightComponent>(settings);
        m_Dirty = true;
        return light;
    }

    Scene::GameObject* EditorScene::InstantiateModel(Assets::AssetID model, Scene::GameObject* parent,
                                                     const Vec3& position) {
        if (m_Assets.GetType(model) != Assets::AssetType::Model) {
            VKE_CORE_ERROR("Editor: {} isn't a model", model.ToString());
            return nullptr;
        }
        Scene::GameObject* root = Scene::InstantiateModel(*m_Scene, m_Assets, m_Assets.LoadModel(model), parent);
        if (!root) return nullptr;
        root->GetTransform().SetLocalPosition(position);
        m_Dirty = true;
        return root;
    }

    void EditorScene::Destroy(Scene::GameObject& object) {
        m_Scene->Destroy(object);
        m_Dirty = true;
    }

} // namespace VKE::Editor
