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
#include <scripting/GameModule.h>

#include <cmath>

namespace VKE::Editor {

namespace {
const Vec3 kWorldUp(0.0f, 1.0f, 0.0f);

// The rotation that points an object's forward axis (-Z) along `direction`.
Quat RotationTowards(const Vec3 &direction) {
	Vec3 forward = glm::normalize(direction);
	Vec3 up = std::abs(forward.y) < 0.999f ? kWorldUp : Vec3(0.0f, 0.0f, 1.0f);
	return glm::quatLookAt(forward, up);
}

const char *GetShapeName(Shape shape) {
	switch (shape) {
		case Shape::Cube:
			return "Cube";
		case Shape::Sphere:
			return "Sphere";
		case Shape::Plane:
			return "Plane";
		case Shape::Cylinder:
			return "Cylinder";
	}
	return "";
}

bool IsInside(const std::filesystem::path &file, const std::filesystem::path &folder) {
	std::error_code ec;
	std::string relative = std::filesystem::weakly_canonical(file, ec)
								   .lexically_relative(std::filesystem::weakly_canonical(folder, ec))
								   .generic_string();
	return !ec && !relative.empty() && relative != "." && relative.rfind("..", 0) != 0;
}

ObjectID GetObjectID(const GameObject *object) {
	return object ? object->GetID() : ObjectID();
}

Handle<GameObject> FindHandle(Scene &scene, ObjectID id) {
	GameObject *object = id.IsValid() ? scene.FindByID(id) : nullptr;
	return object ? object->GetHandle() : Handle<GameObject>();
}
} // namespace

EditorScene::EditorScene(AssetManager &assets) :
		m_Assets(assets),
		m_Prefabs(assets),
		m_PrefabEditing(m_Document, m_Prefabs, assets),
		m_Play(m_Document, m_Prefabs, assets, m_Recovery) {
	m_Document.Objects = MakeIunique<Scene>();
}

EditorScene::~EditorScene() = default;

void EditorScene::Replace(Iunique<Scene> objects, AssetID id, bool dirty) {
	m_Document.Objects = std::move(objects);
	m_Document.ID = id;
	m_Document.Dirty = dirty;
	m_Document.Selected = {};
	++m_OpenCount;
}

void EditorScene::New() {
	Stop();
	Replace(MakeIunique<Scene>(), {}, false);
	Scene &scene = GetScene();

	GameObject &camera = scene.CreateObject("Main camera");
	Vec3 position(0.0f, 1.5f, 5.0f);
	camera.GetTransform().SetLocalPosition(position);
	camera.GetTransform().SetLocalRotation(RotationTowards(Vec3(0.0f, 0.5f, 0.0f) - position));
	camera.AddComponent<CameraComponent>();

	LightSettings sun;
	sun.Color = Vec3(1.0f, 0.96f, 0.9f);
	sun.Intensity = 3.0f;
	sun.CastsShadow = true;
	GameObject &light = scene.CreateObject("Sun");
	light.GetTransform().SetLocalRotation(RotationTowards(Vec3(-0.4f, -1.0f, -0.6f)));
	light.AddComponent<LightComponent>(sun);
	VKE_CORE_INFO("Editor: new scene");
}

bool EditorScene::Open(AssetID scene) {
	if (m_Assets.GetType(scene) != AssetType::Scene) {
		VKE_CORE_ERROR("Editor: {} isn't a scene", scene.ToString());
		return false;
	}
	auto loaded = MakeIunique<Scene>();
	if (!SceneSerializer::Load(*loaded, m_Prefabs, m_Assets.GetFile(scene))) {
		return false;
	}
	Stop();
	Replace(std::move(loaded), scene, false);
	return true;
}

bool EditorScene::Save() {
	if (IsEditingPrefab()) {
		return m_PrefabEditing.Save();
	}
	std::filesystem::path file = GetFile();
	if (file.empty() || !SceneSerializer::Save(GetScene(), m_Prefabs, file)) {
		return false;
	}
	m_Document.Dirty = false;
	return true;
}

bool EditorScene::SaveAs(const std::filesystem::path &file) {
	const AssetRoot &root = m_Assets.GetRoots().front();
	if (root.ReadOnly || !IsInside(file, root.Folder)) {
		VKE_CORE_ERROR("Editor: scenes are saved inside the project's assets folder, '{}'", root.Folder.u8string());
		return false;
	}
	if (!SceneSerializer::Save(GetScene(), m_Prefabs, file)) {
		return false;
	}

	// The file is an asset now; saving over a scene keeps its ID.
	AssetID id = m_Assets.FindAsset(file.u8string());
	if (!id.IsValid()) {
		return false;
	}
	m_Document.ID = id;
	m_Document.Dirty = false;
	return true;
}

EditorMode EditorScene::GetMode() const {
	if (m_Play.IsRunning()) {
		return m_Play.IsPaused() ? EditorMode::Paused : EditorMode::Playing;
	}
	return m_PrefabEditing.IsOpen() ? EditorMode::EditingPrefab : EditorMode::EditingScene;
}

bool EditorScene::Play() {
	if (IsEditingPrefab()) {
		VKE_CORE_ERROR("Editor: Play starts from the scene; close the prefab first");
		return false;
	}
	return m_Play.Play();
}

bool EditorScene::RestoreRecovery() {
	std::string text;
	AssetID id;
	if (!m_Recovery.Read(text, id)) {
		return false;
	}
	auto loaded = MakeIunique<Scene>();
	if (!SceneSerializer::LoadText(*loaded, m_Prefabs, text, "the recovered scene")) {
		return false;
	}
	Replace(std::move(loaded), m_Assets.GetType(id) == AssetType::Scene ? id : AssetID(), true);
	m_Recovery.Discard();
	VKE_CORE_INFO("Editor: restored the unsaved changes of {}", GetFileName());
	return true;
}

bool EditorScene::SwitchGame(
		GameModule &game, const std::filesystem::path &library, const std::filesystem::path &workFolder) {
	if (IsPlaying()) {
		// A scene with unsaved changes has its copy for recovery since Play.
		if (!game.Prepare(library, workFolder)) {
			return false;
		}
		m_Play.SwitchGame(game);
		return true;
	}

	bool prefab = IsEditingPrefab();
	SceneDocument &waiting = m_PrefabEditing.GetWaiting();
	SceneDocument &scene = prefab ? waiting : m_Document;
	std::string sceneText = SceneSerializer::ToText(*scene.Objects, m_Prefabs);
	std::string prefabText =
			prefab ? SceneSerializer::PrefabToText(*GetScene().GetRootObjects().front(), m_Assets) : std::string();
	if (sceneText.empty() || (prefab && prefabText.empty())) {
		VKE_CORE_ERROR("Editor: the scene can't be kept, so the new scripts aren't loaded");
		return false;
	}

	// The new game's globals and constructors run from here on, and they can take the editor down.
	AssetID sceneID = scene.ID;
	bool sceneDirty = scene.Dirty;
	if (sceneDirty) {
		m_Recovery.Write(sceneText, sceneID);
	}
	if (!game.Prepare(library, workFolder)) {
		if (sceneDirty) {
			m_Recovery.Discard();
		}
		return false;
	}

	ObjectID selected = GetObjectID(GetSelected());
	ObjectID waitingSelected = prefab ? GetObjectID(waiting.Objects->Get(waiting.Selected)) : ObjectID();

	m_Document.Objects.reset();
	waiting.Objects.reset();
	m_Prefabs.ForgetAll();
	game.Switch();

	auto loadedScene = MakeIunique<Scene>();
	if (!SceneSerializer::LoadText(*loadedScene, m_Prefabs, sceneText, "the scene before the reload")) {
		// Not saved over what's on disk; the copy for recovery stays for the next start.
		VKE_CORE_ERROR("Editor: the scene can't be made again after the reload, and the editor keeps it empty");
		waiting = {};
		m_Document = {};
		m_Document.Objects = std::move(loadedScene);
		return false;
	}

	auto loadedPrefab = MakeIunique<Scene>();
	bool prefabLoaded =
			prefab && SceneSerializer::LoadPrefabText(*loadedPrefab, m_Assets, prefabText, "the open prefab");
	if (prefab && (!prefabLoaded || loadedPrefab->GetRootObjects().empty())) {
		VKE_CORE_ERROR("Editor: the open prefab can't be made again after the reload, and it was closed");
		waiting = {};
		m_Document.Objects = std::move(loadedScene);
		m_Document.ID = sceneID;
		m_Document.Dirty = sceneDirty;
		m_Document.Selected = FindHandle(GetScene(), waitingSelected);
		return false;
	}

	if (prefab) {
		waiting.Selected = FindHandle(*loadedScene, waitingSelected);
		waiting.Objects = std::move(loadedScene);
		m_Document.Objects = std::move(loadedPrefab);
	} else {
		m_Document.Objects = std::move(loadedScene);
	}
	m_Document.Selected = FindHandle(GetScene(), selected);
	if (sceneDirty) {
		m_Recovery.Discard();
	}
	return true;
}

void EditorScene::Update(f32 deltaTime) {
	if (GetMode() == EditorMode::Playing) {
		m_Play.Update(deltaTime);
	} else {
		GetScene().Sync();
	}
}

std::filesystem::path EditorScene::GetFile() const {
	return m_Document.ID.IsValid() ? m_Assets.GetFile(m_Document.ID) : std::filesystem::path();
}

bool EditorScene::OpenPrefab(AssetID prefab) {
	if (IsPlaying()) {
		VKE_CORE_ERROR("Editor: a prefab can't be opened while playing");
		return false;
	}
	if (!m_PrefabEditing.Open(prefab)) {
		return false;
	}
	++m_OpenCount;
	m_SelectedAsset = {};
	return true;
}

void EditorScene::ClosePrefab() {
	if (!IsEditingPrefab()) {
		return;
	}
	m_PrefabEditing.Close();
	m_SelectedAsset = {};
	++m_OpenCount;
}

AssetID EditorScene::TakePrefabToOpen() {
	AssetID prefab = m_PrefabToOpen;
	m_PrefabToOpen = {};
	return prefab;
}

bool EditorScene::CanCreatePrefab(const GameObject &object) const {
	return !IsPlaying() && !IsEditingPrefab() && !PrefabLibrary::HasParts(object);
}

AssetID EditorScene::CreatePrefab(GameObject &object, const std::filesystem::path &file) {
	if (!CanCreatePrefab(object)) {
		VKE_CORE_ERROR("Editor: '{}' can't become a prefab", object.GetName());
		return {};
	}
	AssetID prefab = m_Prefabs.Create(object, file);
	if (!prefab.IsValid()) {
		return {};
	}

	// The IDs of the objects are their parts' now, which says what to find in the instance.
	Scene &scene = GetScene();
	auto isInside = [&](const GameObject *other) { return other && (other == &object || other->IsChildOf(&object)); };
	CameraComponent *camera = scene.GetMainCamera();
	ObjectID cameraPart = camera && isInside(&camera->GetOwner()) ? camera->GetOwner().GetID() : ObjectID();
	ObjectID selectedPart = isInside(GetSelected()) ? GetSelected()->GetID() : ObjectID();
	GameObject *parent = object.GetParent();
	size_t index = object.GetIndexInParent();
	ObjectID id = object.GetID();

	scene.Destroy(object);
	GameObject *instance = m_Prefabs.Instantiate(scene, prefab, parent, id);
	if (!instance) {
		return {};
	}
	instance->SetIndexInParent(index);
	if (cameraPart.IsValid()) {
		GameObject *part = PrefabLibrary::FindInstancePart(*instance, cameraPart);
		scene.SetMainCamera(part ? part->GetComponent<CameraComponent>() : nullptr);
	}
	if (selectedPart.IsValid()) {
		Select(PrefabLibrary::FindInstancePart(*instance, selectedPart));
	}
	m_Document.Dirty = true;
	VKE_CORE_INFO("Editor: created the prefab {}", file.filename().u8string());
	return prefab;
}

bool EditorScene::ChangePrefab(AssetID prefab, const std::function<void(GameObject &)> &change) {
	GameObject *root = m_Prefabs.GetRoot(prefab);
	if (!root || !m_Assets.IsWritable(prefab)) {
		VKE_CORE_ERROR("Editor: the prefab {} can't be written", prefab.ToString());
		return false;
	}
	std::string text = SceneSerializer::ToText(GetScene(), m_Prefabs);
	if (text.empty()) {
		return false;
	}

	change(*root);
	bool saved = m_Prefabs.Save(prefab);
	m_Prefabs.Forget(prefab);
	return Reload(m_Document, m_Prefabs, text, "the scene's instances") && saved;
}

void EditorScene::Unpack(GameObject &instance) {
	PrefabLibrary::Unpack(instance);
	m_Document.Dirty = true;
}

bool EditorScene::IsFixed(const GameObject &object) const {
	if (object.IsPrefabPart() && !object.IsPrefabRoot()) {
		return true;
	}
	return IsEditingPrefab() && !object.GetParent();
}

bool EditorScene::AcceptsChildren(const GameObject *parent) const {
	return parent ? !parent->IsPrefabPart() : !IsEditingPrefab();
}

GameObject *EditorScene::GetDefaultParent() const {
	return IsEditingPrefab() ? GetScene().GetRootObjects().front() : nullptr;
}

GameObject *EditorScene::GetSelected() const {
	return GetScene().Get(m_Document.Selected);
}

void EditorScene::Select(GameObject *object) {
	m_Document.Selected = object ? object->GetHandle() : Handle<GameObject>();
	m_SelectedAsset = {};
}

void EditorScene::SelectAsset(AssetID asset) {
	m_Document.Selected = {};
	m_SelectedAsset = asset;
}

GameObject &EditorScene::CreateEmpty(GameObject *parent) {
	m_Document.Dirty = true;
	return GetScene().CreateObject("GameObject", parent ? parent : GetDefaultParent());
}

GameObject *EditorScene::CreateShape(Shape shape, GameObject *parent) {
	const char *name = GetShapeName(shape);
	std::filesystem::path content = FileSystem::GetEngineContentDirectory();
	Handle<ModelData> model = m_Assets.LoadModel((content / "Shapes" / (std::string(name) + ".obj")).u8string());
	const ModelData *data = m_Assets.GetModel(model);
	Handle<MeshData> mesh;
	for (size_t i = 0; data && i < data->Nodes.size() && !mesh.IsValid(); ++i) {
		mesh = data->Nodes[i].Mesh;
	}
	if (!mesh.IsValid()) {
		VKE_CORE_ERROR("Editor: the engine's content has no {} shape", name);
		return nullptr;
	}

	GameObject &object = GetScene().CreateObject(name, parent ? parent : GetDefaultParent());
	auto *component = object.AddComponent<MeshComponent>(mesh);
	Handle<MaterialData> material = m_Assets.LoadMaterial((content / "Materials" / "Default.mat").u8string());
	if (material.IsValid()) {
		component->SetMaterials({ material });
	}
	m_Document.Dirty = true;
	return &object;
}

GameObject &EditorScene::CreateCamera(GameObject *parent) {
	GameObject &camera = GetScene().CreateObject("Camera", parent ? parent : GetDefaultParent());
	camera.GetTransform().SetLocalPosition(Vec3(0.0f, 1.0f, 5.0f));
	camera.AddComponent<CameraComponent>();
	m_Document.Dirty = true;
	return camera;
}

GameObject &EditorScene::CreateLight(LightType type, GameObject *parent) {
	LightSettings settings;
	settings.Type = type;
	const char *name = "Directional light";
	Vec3 position(0.0f);
	Vec3 direction(-0.4f, -1.0f, -0.6f);
	if (type == LightType::Point) {
		name = "Point light";
		position = Vec3(0.0f, 2.0f, 0.0f);
		settings.Intensity = 20.0f;
	} else if (type == LightType::Spot) {
		name = "Spot light";
		position = Vec3(0.0f, 3.0f, 0.0f);
		direction = Vec3(0.0f, -1.0f, 0.0f);
		settings.Intensity = 40.0f;
	}

	GameObject &light = GetScene().CreateObject(name, parent ? parent : GetDefaultParent());
	light.GetTransform().SetLocalPosition(position);
	light.GetTransform().SetLocalRotation(RotationTowards(direction));
	light.AddComponent<LightComponent>(settings);
	m_Document.Dirty = true;
	return light;
}

GameObject *EditorScene::InstantiateModel(AssetID model, GameObject *parent, const Vec3 &position) {
	if (m_Assets.GetType(model) != AssetType::Model) {
		VKE_CORE_ERROR("Editor: {} isn't a model", model.ToString());
		return nullptr;
	}
	GameObject *root = VKE::InstantiateModel(
			GetScene(), m_Assets, m_Assets.LoadModel(model), parent ? parent : GetDefaultParent());
	if (!root) {
		return nullptr;
	}
	root->GetTransform().SetLocalPosition(position);
	m_Document.Dirty = true;
	return root;
}

bool EditorScene::CanPlace(AssetID asset) const {
	std::optional<AssetType> type = m_Assets.GetType(asset);
	return type == AssetType::Model || (type == AssetType::Prefab && !IsEditingPrefab());
}

GameObject *EditorScene::Place(AssetID asset, GameObject *parent, const Vec3 &position) {
	if (m_Assets.GetType(asset) != AssetType::Prefab) {
		return InstantiateModel(asset, parent, position);
	}
	GameObject *instance = m_Prefabs.Instantiate(GetScene(), asset, parent, ObjectID::Generate());
	if (!instance) {
		return nullptr;
	}
	instance->GetTransform().SetLocalPosition(position);
	m_Document.Dirty = true;
	return instance;
}

void EditorScene::Destroy(GameObject &object) {
	GetScene().Destroy(object);
	m_Document.Dirty = true;
}

} // namespace VKE::Editor
