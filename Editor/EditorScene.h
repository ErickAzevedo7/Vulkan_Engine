#pragma once

#include "PlaySession.h"
#include "PrefabEditing.h"
#include "SceneDocument.h"
#include "SceneRecovery.h"

#include <assets/AssetTypes.h>
#include <core/Types.h>
#include <math/MathTypes.h>
#include <renderer/RenderTypes.h>
#include <scene/Prefab.h>

#include <filesystem>
#include <functional>
#include <string>

namespace VKE {
class AssetManager;
class GameModule;
class GameObject;
class Scene;
} // namespace VKE

namespace VKE::Editor {

// The engine's shapes, in Content/Shapes.
enum class Shape {
	Cube,
	Sphere,
	Plane,
	Cylinder,
};

// What the editor is doing with the open objects. One at a time: Play starts only from the
// scene, and a prefab opens only outside Play.
enum class EditorMode {
	EditingScene,
	EditingPrefab,
	Playing,
	// Playing, stopped in time.
	Paused,
};

// The scene open in the editor: its objects, the scene asset it was opened from and is
// saved to, whether it changed since, and what is selected: an object or an asset.
//
// It moves between the modes: the game runs in a PlaySession, and a prefab opens in place of
// the scene through PrefabEditing.
class EditorScene {
public:
	explicit EditorScene(AssetManager &assets);
	~EditorScene();

	// An unsaved scene with a camera and a light.
	void New();
	// False, logged, if the asset isn't a scene or can't be read; the open scene stays.
	bool Open(AssetID scene);
	// Writes the scene to its file. False if it has none yet or the write fails.
	bool Save();
	// Writes the scene to `file`, which must be inside the project's assets, and makes it
	// the scene's file. False, logged, if it isn't or the write fails.
	bool SaveAs(const std::filesystem::path &file);

	Scene &GetScene() { return *m_Document.Objects; }
	const Scene &GetScene() const { return *m_Document.Objects; }
	AssetManager &GetAssets() { return m_Assets; }
	PrefabLibrary &GetPrefabs() { return m_Prefabs; }

	// Invalid until the scene is saved. While a prefab is open, these are the prefab's.
	AssetID GetID() const { return m_Document.ID; }
	// Where the scene asset is now, which follows it when it is moved; empty until saved, or
	// once its file is deleted.
	std::filesystem::path GetFile() const;
	// "Sponza.scene", or "Untitled" until saved.
	std::string GetFileName() const { return Editor::GetFileName(m_Assets, m_Document.ID); }

	// While playing, whether it had changed when Play was pressed: nothing done since counts.
	bool IsDirty() const { return IsPlaying() ? m_Play.WasDirtyAtPlay() : m_Document.Dirty; }
	void MarkDirty() { m_Document.Dirty = true; }

	EditorMode GetMode() const;
	// True while playing or paused.
	bool IsPlaying() const { return m_Play.IsRunning(); }
	bool IsEditingPrefab() const { return m_PrefabEditing.IsOpen(); }

	// False, logged, while a prefab is open or if the scene can't be kept.
	bool Play();
	void Stop() { m_Play.Stop(); }
	// Pause, input and faults of the running game.
	PlaySession &GetPlay() { return m_Play; }

	// Where the scene is kept during Play if it has unsaved changes.
	SceneRecovery &GetRecovery() { return m_Recovery; }
	// Opens the scene kept there, with its changes unsaved. False, logged, if it can't be read.
	bool RestoreRecovery();

	// Replaces the game with the DLL `library`, as GameModule::Prepare takes it. In Play the
	// objects stay and the scripts on them change. Otherwise every object is destroyed and made
	// again from what it held, including a prefab open now, unsaved changes and selection.
	// False, logged, if something can't be kept or the DLL can't be loaded, and then the
	// running game stays if it can.
	bool SwitchGame(GameModule &game, const std::filesystem::path &library, const std::filesystem::path &workFolder);

	// Runs the frame's update for the mode, `deltaTime` seconds after the last. Call it once
	// per frame, before rendering.
	void Update(f32 deltaTime);

	// Changes when another scene is opened, but not when Stop restores this one.
	u32 GetOpenCount() const { return m_OpenCount; }

	// False, logged, while playing, if the asset isn't a prefab or can't be read; the scene stays.
	bool OpenPrefab(AssetID prefab);
	// Back to the scene, dropping what wasn't saved of the prefab.
	void ClosePrefab();
	// The name of the scene waiting while a prefab is open.
	std::string GetWaitingFileName() const { return m_PrefabEditing.GetWaitingFileName(); }

	// Asks for a prefab to be opened, for what can't replace the scene while drawing it.
	void AskToOpenPrefab(AssetID prefab) { m_PrefabToOpen = prefab; }
	// The prefab asked for since the last call; invalid if none.
	AssetID TakePrefabToOpen();

	// Whether `object` could become a prefab: prefabs don't go inside prefabs.
	bool CanCreatePrefab(const GameObject &object) const;
	// Writes `object` and everything below it to `file`, and puts an instance of the new
	// prefab in their place. Invalid, logged, if it can't.
	AssetID CreatePrefab(GameObject &object, const std::filesystem::path &file);
	// Has `change` edit the prefab's objects, given by their root, writes the file and remakes
	// the scene's instances, which keep their overrides. Replaces every object of the scene.
	// False, logged, if the prefab can't be written.
	bool ChangePrefab(AssetID prefab, const std::function<void(GameObject &)> &change);
	// Makes an instance plain objects, which stay as they are, and marks the scene changed.
	void Unpack(GameObject &instance);

	// The hierarchy's rules. Prefab instances keep the shape of their prefab: only a whole
	// instance moves or goes away, and nothing goes inside one. A prefab has one root.
	// Whether `object` has to stay where it is and can't be destroyed.
	bool IsFixed(const GameObject &object) const;
	// Whether objects can go under `parent`, or at the root of the scene for null.
	bool AcceptsChildren(const GameObject *parent) const;
	// Where new objects go when no parent is given: the open prefab's root, or the scene's root.
	GameObject *GetDefaultParent() const;

	// The selected object, or null. Nothing is selected after New and Open.
	GameObject *GetSelected() const;
	// Selecting anything, or nothing, deselects the asset.
	void Select(GameObject *object);
	// The selected asset, or invalid. Selecting one deselects the object.
	AssetID GetSelectedAsset() const { return m_SelectedAsset; }
	void SelectAsset(AssetID asset);

	// New objects go last under `parent`, or at the root of the scene, and mark it changed.
	GameObject &CreateEmpty(GameObject *parent = nullptr);
	// Null, logged, if the engine's content is missing the shape.
	GameObject *CreateShape(Shape shape, GameObject *parent = nullptr);
	GameObject &CreateCamera(GameObject *parent = nullptr);
	GameObject &CreateLight(LightType type, GameObject *parent = nullptr);
	// The objects of a model asset, last under `parent` or at the root of the scene, with the
	// root at `position` in its parent's space. Null, logged, if it isn't a model or can't load.
	GameObject *InstantiateModel(AssetID model, GameObject *parent = nullptr, const Vec3 &position = Vec3(0.0f));
	// Whether the asset can go in the scene with Place: a model, or a prefab outside a prefab.
	bool CanPlace(AssetID asset) const;
	// The objects of a model or a new instance of a prefab, as InstantiateModel does.
	GameObject *Place(AssetID asset, GameObject *parent = nullptr, const Vec3 &position = Vec3(0.0f));
	// Destroys the object and its children, and marks the scene changed.
	void Destroy(GameObject &object);

private:
	// Makes `objects` the open scene, saved as `id`, with nothing selected.
	void Replace(Iunique<Scene> objects, AssetID id, bool dirty);

	AssetManager &m_Assets;
	PrefabLibrary m_Prefabs;
	SceneDocument m_Document;
	u32 m_OpenCount = 0;
	AssetID m_SelectedAsset;
	AssetID m_PrefabToOpen;
	SceneRecovery m_Recovery;
	// It holds the scene waiting while a prefab is open.
	PrefabEditing m_PrefabEditing;
	// Last: it stops the scripts of a scene that is still there.
	PlaySession m_Play;
};

} // namespace VKE::Editor
