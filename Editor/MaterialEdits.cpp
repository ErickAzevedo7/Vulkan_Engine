#include "MaterialEdits.h"

#include <assets/AssetManager.h>
#include <core/Log.h>

#include <vector>

namespace VKE::Editor {

MaterialEdits::MaterialEdits(AssetManager &assets) : m_Assets(assets) {}

bool MaterialEdits::Change(AssetID material, MaterialData values) {
	if (m_Assets.GetType(material) != AssetType::Material || !m_Assets.IsWritable(material)) {
		return false;
	}
	Handle<MaterialData> handle = m_Assets.LoadMaterial(material);
	if (!handle.IsValid()) {
		return false;
	}
	m_Assets.UpdateMaterial(handle, std::move(values));
	m_Unsaved.insert(material);
	return true;
}

bool MaterialEdits::Save(AssetID material) {
	if (!m_Assets.SaveMaterial(material)) {
		VKE_CORE_ERROR("Editor: couldn't save the material {}", m_Assets.GetFile(material).u8string());
		return false;
	}
	m_Unsaved.erase(material);
	VKE_CORE_INFO("Editor: saved {}", m_Assets.GetFile(material).filename().u8string());
	return true;
}

bool MaterialEdits::SaveAll() {
	// Copied: saving takes them out of the set.
	std::vector<AssetID> unsaved(m_Unsaved.begin(), m_Unsaved.end());
	bool saved = true;
	for (AssetID material : unsaved) {
		saved &= Save(material);
	}
	return saved;
}

void MaterialEdits::Revert(AssetID material) {
	if (m_Assets.ReloadMaterial(material)) {
		m_Unsaved.erase(material);
	}
}

void MaterialEdits::DropMissing() {
	for (auto it = m_Unsaved.begin(); it != m_Unsaved.end();) {
		it = m_Assets.GetType(*it) ? std::next(it) : m_Unsaved.erase(it);
	}
}

} // namespace VKE::Editor
