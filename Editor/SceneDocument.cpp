#include "SceneDocument.h"

#include <assets/AssetManager.h>
#include <scene/GameObject.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>

#include <filesystem>

namespace VKE::Editor {

SceneDocument::SceneDocument() = default;
SceneDocument::~SceneDocument() = default;
SceneDocument::SceneDocument(SceneDocument &&) noexcept = default;
SceneDocument &SceneDocument::operator=(SceneDocument &&) noexcept = default;

bool Reload(SceneDocument &document, PrefabLibrary &prefabs, const std::string &text, const std::string &name) {
	GameObject *current = document.Objects->Get(document.Selected);
	ObjectID selectedID = current ? current->GetID() : ObjectID();

	auto loaded = MakeIunique<Scene>();
	if (!SceneSerializer::LoadText(*loaded, prefabs, text, name)) {
		return false;
	}
	document.Objects = std::move(loaded);
	GameObject *same = selectedID.IsValid() ? document.Objects->FindByID(selectedID) : nullptr;
	document.Selected = same ? same->GetHandle() : Handle<GameObject>();
	return true;
}

std::string GetFileName(const AssetManager &assets, AssetID id) {
	std::filesystem::path file = id.IsValid() ? assets.GetFile(id) : std::filesystem::path();
	return file.empty() ? std::string("Untitled") : file.filename().u8string();
}

} // namespace VKE::Editor
