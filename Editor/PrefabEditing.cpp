#include "PrefabEditing.h"

#include <assets/AssetManager.h>
#include <core/Log.h>
#include <scene/GameObject.h>
#include <scene/Prefab.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>

namespace VKE::Editor {

PrefabEditing::PrefabEditing(SceneDocument &document, PrefabLibrary &prefabs, AssetManager &assets) :
		m_Document(document), m_Prefabs(prefabs), m_Assets(assets) {}

bool PrefabEditing::Open(AssetID prefab) {
	if (m_Assets.GetType(prefab) != AssetType::Prefab) {
		VKE_CORE_ERROR("Editor: {} isn't a prefab", prefab.ToString());
		return false;
	}
	auto objects = MakeIunique<Scene>();
	std::filesystem::path file = m_Assets.GetFile(prefab);
	if (!SceneSerializer::LoadPrefab(*objects, m_Assets, file) || objects->GetRootObjects().empty()) {
		VKE_CORE_ERROR("Editor: '{}' can't be read", file.u8string());
		return false;
	}

	m_Waiting = std::move(m_Document);
	m_Document = {};
	m_Document.Objects = std::move(objects);
	m_Document.ID = prefab;
	m_Document.Selected = m_Document.Objects->GetRootObjects().front()->GetHandle();
	VKE_CORE_INFO("Editor: editing {}", GetFileName(m_Assets, prefab));
	return true;
}

void PrefabEditing::Close() {
	if (IsOpen()) {
		m_Document = std::move(m_Waiting);
		m_Waiting = {};
	}
}

bool PrefabEditing::Save() {
	std::filesystem::path file = m_Document.ID.IsValid() ? m_Assets.GetFile(m_Document.ID) : std::filesystem::path();
	if (file.empty() || !m_Assets.IsWritable(m_Document.ID)) {
		VKE_CORE_ERROR("Editor: {} can't be written", GetFileName(m_Assets, m_Document.ID));
		return false;
	}
	// The scene as it is against the prefab before the change, to keep the overrides.
	std::string text = SceneSerializer::ToText(*m_Waiting.Objects, m_Prefabs);
	if (text.empty()) {
		return false;
	}
	if (!SceneSerializer::SavePrefab(*m_Document.Objects->GetRootObjects().front(), m_Assets, file)) {
		return false;
	}

	m_Prefabs.Forget(m_Document.ID);
	m_Document.Dirty = false;
	return Reload(m_Waiting, m_Prefabs, text, "the scene's instances");
}

std::string PrefabEditing::GetWaitingFileName() const {
	return GetFileName(m_Assets, m_Waiting.ID);
}

} // namespace VKE::Editor
