#pragma once

#include "SceneDocument.h"

#include <assets/AssetTypes.h>

#include <string>

namespace VKE {
class AssetManager;
class PrefabLibrary;
} // namespace VKE

namespace VKE::Editor {

// A prefab open in place of the scene, which waits in memory: the open objects are then the
// prefab's, and Save writes the prefab and remakes the scene's instances from it.
class PrefabEditing {
public:
	PrefabEditing(SceneDocument &document, PrefabLibrary &prefabs, AssetManager &assets);

	bool IsOpen() const { return m_Waiting.Objects != nullptr; }
	// False, logged, if the asset isn't a prefab or can't be read; the scene stays.
	bool Open(AssetID prefab);
	// Back to the scene, dropping what wasn't saved of the prefab.
	void Close();
	// Writes the prefab and remakes the waiting scene's instances, which keep their overrides.
	bool Save();

	// The name of the scene waiting while a prefab is open.
	std::string GetWaitingFileName() const;
	// The scene waiting while a prefab is open; it has no objects otherwise.
	SceneDocument &GetWaiting() { return m_Waiting; }

private:
	SceneDocument &m_Document;
	PrefabLibrary &m_Prefabs;
	AssetManager &m_Assets;
	SceneDocument m_Waiting;
};

} // namespace VKE::Editor
