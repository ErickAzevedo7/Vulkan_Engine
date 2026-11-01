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

#include <yaml-cpp/yaml.h>

#include <cmath>
#include <fstream>

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

    EditorScene::EditorScene(Assets::AssetManager& assets)
        : m_Assets(assets), m_Prefabs(assets), m_Scene(MakeIunique<Scene::Scene>()) {
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
        if (!Scene::SceneSerializer::Load(*loaded, m_Prefabs, file)) return false;

        m_Scene = std::move(loaded);
        m_ID = scene;
        ++m_OpenCount;
        m_PlayMode = PlayMode::Edit;
        m_Dirty = false;
        m_Selected = {};
        return true;
    }

    bool EditorScene::Save() {
        if (IsEditingPrefab()) return SavePrefab();
        std::filesystem::path file = GetFile();
        if (file.empty()) return false;
        if (!Scene::SceneSerializer::Save(*m_Scene, m_Prefabs, file)) return false;
        m_Dirty = false;
        return true;
    }

    bool EditorScene::SaveAs(const std::filesystem::path& file) {
        const Assets::AssetRoot& root = m_Assets.GetRoots().front();
        if (root.ReadOnly || !IsInside(file, root.Folder)) {
            VKE_CORE_ERROR("Editor: scenes are saved inside the project's assets folder, '{}'", root.Folder.u8string());
            return false;
        }
        if (!Scene::SceneSerializer::Save(*m_Scene, m_Prefabs, file)) return false;

        // The file is an asset now; saving over a scene keeps its ID.
        Assets::AssetID id = m_Assets.FindAsset(file.u8string());
        if (!id.IsValid()) return false;
        m_ID = id;
        m_Dirty = false;
        return true;
    }

    bool EditorScene::Play() {
        if (IsPlaying()) return true;
        m_PlaySnapshot = Scene::SceneSerializer::ToText(*m_Scene, m_Prefabs);
        if (m_PlaySnapshot.empty()) return false;
        m_DirtyAtPlay = m_Dirty;
        m_PlayMode = PlayMode::Playing;
        if (m_Dirty) WriteRecovery(m_PlaySnapshot);
        m_Runner.Start(*m_Scene, MakeGameContext());
        VKE_CORE_INFO("Editor: Play: scene kept in memory ({} objects); Stop restores it", m_Scene->GetObjectCount());
        return true;
    }

    Scripting::GameContext EditorScene::MakeGameContext() {
        return {m_Prefabs, *m_Input, [this](Assets::AssetID scene) { SwitchPlayedScene(scene); }, [this] { Stop(); },
                [this](const std::string& message) {
                    m_Fault = message;
                    Stop();
                }};
    }

    void EditorScene::WriteRecovery(const std::string& text) const {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Scene" << YAML::Value << YAML::DoubleQuoted << m_ID.ToString();
        out << YAML::Key << "Text" << YAML::Value << YAML::Literal << text;
        out << YAML::EndMap;
        std::ofstream stream(m_RecoveryFile, std::ios::binary | std::ios::trunc);
        stream << out.c_str() << '\n';
        if (!stream) VKE_CORE_WARN("Editor: couldn't write '{}'", m_RecoveryFile.u8string());
    }

    bool EditorScene::HasRecovery() const {
        std::error_code ec;
        return std::filesystem::exists(m_RecoveryFile, ec);
    }

    bool EditorScene::RestoreRecovery() {
        YAML::Node root;
        try {
            root = YAML::LoadFile(m_RecoveryFile.string());
        } catch (const YAML::Exception& error) {
            VKE_CORE_ERROR("Editor: the recovered scene can't be read: {}", error.what());
            return false;
        }
        std::string text = root["Text"] && root["Text"].IsScalar() ? root["Text"].Scalar() : std::string();
        auto loaded = MakeIunique<Scene::Scene>();
        if (!Scene::SceneSerializer::LoadText(*loaded, m_Prefabs, text, "the recovered scene")) return false;

        Assets::AssetID id;
        if (root["Scene"] && root["Scene"].IsScalar()) id = Assets::AssetID::Parse(root["Scene"].Scalar());
        m_Scene = std::move(loaded);
        m_ID = m_Assets.GetType(id) == Assets::AssetType::Scene ? id : Assets::AssetID();
        ++m_OpenCount;
        m_Dirty = true;
        m_Selected = {};
        DiscardRecovery();
        VKE_CORE_INFO("Editor: restored the unsaved changes of {}", GetFileName());
        return true;
    }

    void EditorScene::DiscardRecovery() {
        std::error_code ec;
        std::filesystem::remove(m_RecoveryFile, ec);
    }

    std::string EditorScene::TakeFault() {
        std::string fault;
        fault.swap(m_Fault);
        return fault;
    }

    void EditorScene::SwitchPlayedScene(Assets::AssetID scene) {
        auto loaded = MakeIunique<Scene::Scene>();
        if (m_Assets.GetType(scene) != Assets::AssetType::Scene ||
            !Scene::SceneSerializer::Load(*loaded, m_Prefabs, m_Assets.GetFile(scene))) {
            VKE_CORE_ERROR("Editor: the game asked for scene {}, which can't be loaded", scene.ToString());
            return;
        }
        m_Runner.Stop();
        m_Scene = std::move(loaded);
        m_Selected = {};
        m_Runner.Start(*m_Scene, MakeGameContext());
    }

    void EditorScene::TogglePause() {
        if (!IsPlaying()) return;
        m_PlayMode = m_PlayMode == PlayMode::Playing ? PlayMode::Paused : PlayMode::Playing;
    }

    void EditorScene::Stop() {
        if (!IsPlaying()) return;
        m_Runner.Stop();
        DiscardRecovery();
        if (!Reload(m_Scene, m_Selected, m_PlaySnapshot, "the scene kept at Play")) {
            VKE_CORE_ERROR("Editor: the scene kept at Play can't be restored; what is open stays");
            return;
        }
        m_PlaySnapshot.clear();
        m_PlayMode = PlayMode::Edit;
        m_Dirty = m_DirtyAtPlay;
        VKE_CORE_INFO("Editor: Stop: scene restored");
    }

    bool EditorScene::Reload(Iunique<Scene::Scene>& scene, Handle<Scene::GameObject>& selected, const std::string& text,
                             const std::string& name) {
        Scene::GameObject* current = scene->Get(selected);
        ObjectID selectedID = current ? current->GetID() : ObjectID();

        auto loaded = MakeIunique<Scene::Scene>();
        if (!Scene::SceneSerializer::LoadText(*loaded, m_Prefabs, text, name)) return false;
        scene = std::move(loaded);
        Scene::GameObject* same = selectedID.IsValid() ? scene->FindByID(selectedID) : nullptr;
        selected = same ? same->GetHandle() : Handle<Scene::GameObject>();
        return true;
    }

    void EditorScene::Update(f32 deltaTime) {
        if (m_PlayMode == PlayMode::Playing) {
            m_Runner.Update(deltaTime);
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

    std::string EditorScene::GetWaitingFileName() const {
        std::filesystem::path file = m_Waiting.ID.IsValid() ? m_Assets.GetFile(m_Waiting.ID) : std::filesystem::path();
        return file.empty() ? std::string("Untitled") : file.filename().u8string();
    }

    bool EditorScene::OpenPrefab(Assets::AssetID prefab) {
        if (m_Assets.GetType(prefab) != Assets::AssetType::Prefab) {
            VKE_CORE_ERROR("Editor: {} isn't a prefab", prefab.ToString());
            return false;
        }
        auto objects = MakeIunique<Scene::Scene>();
        std::filesystem::path file = m_Assets.GetFile(prefab);
        if (!Scene::SceneSerializer::LoadPrefab(*objects, m_Assets, file) || objects->GetRootObjects().empty()) {
            VKE_CORE_ERROR("Editor: '{}' can't be read", file.u8string());
            return false;
        }

        m_Waiting.Objects = std::move(m_Scene);
        m_Waiting.ID = m_ID;
        m_Waiting.Dirty = m_Dirty;
        m_Waiting.Selected = m_Selected;
        m_Scene = std::move(objects);
        m_ID = prefab;
        m_Dirty = false;
        ++m_OpenCount;
        m_Selected = m_Scene->GetRootObjects().front()->GetHandle();
        m_SelectedAsset = {};
        VKE_CORE_INFO("Editor: editing {}", GetFileName());
        return true;
    }

    void EditorScene::ClosePrefab() {
        if (!IsEditingPrefab()) return;
        m_Scene = std::move(m_Waiting.Objects);
        m_ID = m_Waiting.ID;
        m_Dirty = m_Waiting.Dirty;
        m_Selected = m_Waiting.Selected;
        m_SelectedAsset = {};
        m_Waiting = {};
        ++m_OpenCount;
    }

    bool EditorScene::SavePrefab() {
        std::filesystem::path file = GetFile();
        if (file.empty() || !m_Assets.IsWritable(m_ID)) {
            VKE_CORE_ERROR("Editor: {} can't be written", GetFileName());
            return false;
        }
        // The scene as it is against the prefab before the change, to keep the overrides.
        std::string text = Scene::SceneSerializer::ToText(*m_Waiting.Objects, m_Prefabs);
        if (text.empty()) return false;
        if (!Scene::SceneSerializer::SavePrefab(*m_Scene->GetRootObjects().front(), m_Assets, file)) return false;

        m_Prefabs.Forget(m_ID);
        m_Dirty = false;
        return Reload(m_Waiting.Objects, m_Waiting.Selected, text, "the scene's instances");
    }

    Assets::AssetID EditorScene::TakePrefabToOpen() {
        Assets::AssetID prefab = m_PrefabToOpen;
        m_PrefabToOpen = {};
        return prefab;
    }

    bool EditorScene::CanCreatePrefab(const Scene::GameObject& object) const {
        return !IsPlaying() && !IsEditingPrefab() && !Scene::PrefabLibrary::HasParts(object);
    }

    Assets::AssetID EditorScene::CreatePrefab(Scene::GameObject& object, const std::filesystem::path& file) {
        if (!CanCreatePrefab(object)) {
            VKE_CORE_ERROR("Editor: '{}' can't become a prefab", object.GetName());
            return {};
        }
        Assets::AssetID prefab = m_Prefabs.Create(object, file);
        if (!prefab.IsValid()) return {};

        // The IDs of the objects are their parts' now, which says what to find in the instance.
        auto isInside = [&](const Scene::GameObject* other) {
            return other && (other == &object || other->IsChildOf(&object));
        };
        Scene::CameraComponent* camera = m_Scene->GetMainCamera();
        ObjectID cameraPart = camera && isInside(&camera->GetOwner()) ? camera->GetOwner().GetID() : ObjectID();
        ObjectID selectedPart = isInside(GetSelected()) ? GetSelected()->GetID() : ObjectID();
        Scene::GameObject* parent = object.GetParent();
        size_t index = object.GetIndexInParent();
        ObjectID id = object.GetID();

        m_Scene->Destroy(object);
        Scene::GameObject* instance = m_Prefabs.Instantiate(*m_Scene, prefab, parent, id);
        if (!instance) return {};
        instance->SetIndexInParent(index);
        if (cameraPart.IsValid()) {
            Scene::GameObject* part = Scene::PrefabLibrary::FindInstancePart(*instance, cameraPart);
            m_Scene->SetMainCamera(part ? part->GetComponent<Scene::CameraComponent>() : nullptr);
        }
        if (selectedPart.IsValid()) Select(Scene::PrefabLibrary::FindInstancePart(*instance, selectedPart));
        m_Dirty = true;
        VKE_CORE_INFO("Editor: created the prefab {}", file.filename().u8string());
        return prefab;
    }

    bool EditorScene::ChangePrefab(Assets::AssetID prefab, const std::function<void(Scene::GameObject&)>& change) {
        Scene::GameObject* root = m_Prefabs.GetRoot(prefab);
        if (!root || !m_Assets.IsWritable(prefab)) {
            VKE_CORE_ERROR("Editor: the prefab {} can't be written", prefab.ToString());
            return false;
        }
        std::string text = Scene::SceneSerializer::ToText(*m_Scene, m_Prefabs);
        if (text.empty()) return false;

        change(*root);
        bool saved = m_Prefabs.Save(prefab);
        m_Prefabs.Forget(prefab);
        return Reload(m_Scene, m_Selected, text, "the scene's instances") && saved;
    }

    void EditorScene::Unpack(Scene::GameObject& instance) {
        Scene::PrefabLibrary::Unpack(instance);
        m_Dirty = true;
    }

    bool EditorScene::IsFixed(const Scene::GameObject& object) const {
        if (object.IsPrefabPart() && !object.IsPrefabRoot()) return true;
        return IsEditingPrefab() && !object.GetParent();
    }

    bool EditorScene::AcceptsChildren(const Scene::GameObject* parent) const {
        return parent ? !parent->IsPrefabPart() : !IsEditingPrefab();
    }

    Scene::GameObject* EditorScene::GetDefaultParent() const {
        return IsEditingPrefab() ? m_Scene->GetRootObjects().front() : nullptr;
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
        return m_Scene->CreateObject("GameObject", parent ? parent : GetDefaultParent());
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

        Scene::GameObject& object = m_Scene->CreateObject(name, parent ? parent : GetDefaultParent());
        auto* component = object.AddComponent<Scene::MeshComponent>(mesh);
        Handle<Assets::MaterialData> material = m_Assets.LoadMaterial((content / "Materials" / "Default.mat").u8string());
        if (material.IsValid()) component->SetMaterials({material});
        m_Dirty = true;
        return &object;
    }

    Scene::GameObject& EditorScene::CreateCamera(Scene::GameObject* parent) {
        Scene::GameObject& camera = m_Scene->CreateObject("Camera", parent ? parent : GetDefaultParent());
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

        Scene::GameObject& light = m_Scene->CreateObject(name, parent ? parent : GetDefaultParent());
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
        Scene::GameObject* root = Scene::InstantiateModel(*m_Scene, m_Assets, m_Assets.LoadModel(model),
                                                          parent ? parent : GetDefaultParent());
        if (!root) return nullptr;
        root->GetTransform().SetLocalPosition(position);
        m_Dirty = true;
        return root;
    }

    bool EditorScene::CanPlace(Assets::AssetID asset) const {
        std::optional<Assets::AssetType> type = m_Assets.GetType(asset);
        return type == Assets::AssetType::Model || (type == Assets::AssetType::Prefab && !IsEditingPrefab());
    }

    Scene::GameObject* EditorScene::Place(Assets::AssetID asset, Scene::GameObject* parent, const Vec3& position) {
        if (m_Assets.GetType(asset) != Assets::AssetType::Prefab) return InstantiateModel(asset, parent, position);
        Scene::GameObject* instance = m_Prefabs.Instantiate(*m_Scene, asset, parent, ObjectID::Generate());
        if (!instance) return nullptr;
        instance->GetTransform().SetLocalPosition(position);
        m_Dirty = true;
        return instance;
    }

    void EditorScene::Destroy(Scene::GameObject& object) {
        m_Scene->Destroy(object);
        m_Dirty = true;
    }

} // namespace VKE::Editor
