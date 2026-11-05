#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/Types.h>

#include <string>

namespace VKE {
class AssetManager;
class GameObject;
class PrefabLibrary;
class Scene;
} // namespace VKE

namespace VKE::Editor {

// Objects the editor edits and the asset they are saved to: the open scene or prefab, or the
// scene waiting while a prefab is open in its place.
struct SceneDocument {
	Iunique<Scene> Objects;
	// Invalid until saved.
	AssetID ID;
	bool Dirty = false;
	Handle<GameObject> Selected;

	SceneDocument();
	~SceneDocument();
	SceneDocument(SceneDocument &&) noexcept;
	SceneDocument &operator=(SceneDocument &&) noexcept;
};

// Replaces the document's objects with the ones `text` holds, with the same object selected if
// it's there. False, logged, if the text can't be read; the objects stay.
bool Reload(SceneDocument &document, PrefabLibrary &prefabs, const std::string &text, const std::string &name);

// "Sponza.scene", or "Untitled" for an asset that isn't saved or whose file is gone.
std::string GetFileName(const AssetManager &assets, AssetID id);

} // namespace VKE::Editor
