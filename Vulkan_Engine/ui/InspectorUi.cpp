#include "InspectorUi.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

InspectorSelection InspectorUi::selection{};
InspectorPickTarget InspectorUi::pickTarget = InspectorPickTarget::None;

void InspectorUi::selectEntity(int entityId) {
	selection.type = InspectorSelectionType::Entity;
	selection.entityId = entityId;
	// Clear any asset selection state when an entity is picked
	selection.assetPath.clear();

	// Update per-entity selection flags so outline/rendering can use isSelected
	Scene* scene = SceneManager::getActiveScene();
	if (!scene) return;

	auto* entities = scene->getEntities();
	if (!entities) return;

	for (const auto& ePtr : *entities) {
		Entity& e = *ePtr;
		e.isSelected = (static_cast<int>(e.getID()) == entityId);
	}
}

void InspectorUi::selectAsset(const std::string& assetPath) {
	selection.type = InspectorSelectionType::Asset;
	selection.entityId = 0;
	selection.assetPath = assetPath;

	// Clear entity selection flags when switching to an asset selection
	Scene* scene = SceneManager::getActiveScene();
	if (!scene) return;

	auto* entities = scene->getEntities();
	if (!entities) return;

	for (const auto& ePtr : *entities) {
		ePtr->isSelected = false;
	}

	// If we are currently picking something for the inspector, apply it now.
	if (pickTarget != InspectorPickTarget::None) {
		const int selectedId = getSelectedEntityId();
		if (selectedId > 0 && scene &&
			selectedId <= static_cast<int>(scene->getEntityCount())) {
			Entity& entity = scene->getEntity(static_cast<size_t>(selectedId));
			if (pickTarget == InspectorPickTarget::MeshAlbedo) {
				auto* meshComp = entity.getComponent<MeshComponent>();
				if (meshComp) {
					// Derive a logical name for the texture/material from the asset path
					std::filesystem::path p(assetPath);
					std::string stem = p.stem().string();

					Texture* tex = nullptr;
					// Try to get existing texture first
					try {
						tex = TextureManager::getTexture(stem);
					}
					catch (...) {
						// Load new texture and register it in the manager map
						Texture* loaded = TextureManager::loadTexture(
							assetPath, VulkanCore::getDevice(),
							VulkanCore::getPhysicalDevice(),
							VulkanCore::getCommandPool(),
							VulkanCore::getGraphicsQueue());
						TextureManager::createTextureImageView(loaded);
						TextureManager::createTextureSampler(loaded);
						TextureManager::registerTexture(stem, *loaded);
						tex = TextureManager::getTexture(stem);
					}

					// Create or reuse a material that uses this texture
					Material* mat = MaterialManager::getMaterial(stem);
					if (!mat) {
						mat = MaterialManager::createMaterial(stem, stem);
					}

					meshComp->SetMaterial(mat);
				}
			}
		}

		// Reset pick state after handling
		pickTarget = InspectorPickTarget::None;
	}
}

void InspectorUi::clearSelection() {
	selection.type = InspectorSelectionType::None;
	selection.entityId = 0;
	selection.assetPath.clear();

	// Clear all per-entity selection flags
	Scene* scene = SceneManager::getActiveScene();
	if (!scene) return;

	auto* entities = scene->getEntities();
	if (!entities) return;

	for (const auto& ePtr : *entities) {
		ePtr->isSelected = false;
	}
}

int InspectorUi::getSelectedEntityId() {
	if (selection.type == InspectorSelectionType::Entity) {
		return selection.entityId;
	}
	return -1;
}

const std::string& InspectorUi::getSelectedAssetPath() {
	return selection.assetPath;
}

void InspectorUi::render() {
	Scene* scene = SceneManager::getActiveScene();

	ImGui::Begin("Inspector");
	if (selection.type == InspectorSelectionType::Entity && scene &&
		selection.entityId > 0 &&
		selection.entityId <= static_cast<int>(scene->getEntityCount())) {
		Entity& entity =
			scene->getEntity(static_cast<size_t>(selection.entityId));
		char nameBuffer[256];
		strncpy_s(nameBuffer, entity.getName().c_str(), sizeof(nameBuffer));
		nameBuffer[sizeof(nameBuffer) - 1] = 0;

		ImGui::Text("Name");
		ImGui::SameLine();
		if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer))) {
			entity.setName(nameBuffer);
		}

		auto* transform = entity.getComponent<Transform>();
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		if (transform && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::PopStyleVar();
			// Position/rotation/scale controls aligned in two columns
			ImGui::Columns(2, nullptr, false);
			ImGui::SetColumnWidth(0, 60.0f);

			ImVec2 oldPadding = ImGui::GetStyle().FramePadding;
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
			                    ImVec2(oldPadding.x, 0.0f));

			// Position
			ImGui::Text("Position");
			ImGui::NextColumn();
			ImGui::DragFloat3("##Position", &transform->position.x, 0.1f, 0, 0, "%.2f");
			ImGui::NextColumn();

			// Rotation
			ImGui::Text("Rotation");
			ImGui::NextColumn();
			glm::vec3 euler = glm::degrees(glm::eulerAngles(transform->rotation));
			if (ImGui::DragFloat3("##Rotation", &euler.x, 0.5f, 0, 0, "%.2f")) {
				transform->rotation = glm::quat(glm::radians(euler));
			}
			ImGui::NextColumn();

			// Scale
			ImGui::Text("Scale");
			ImGui::NextColumn();
			ImGui::DragFloat3("##Scale", &transform->scale.x, 0.1f, 0, 0, "%.2f");
			ImGui::Columns(1);

			ImGui::PopStyleVar();
		}
		else {
			ImGui::PopStyleVar();
		}

		// Mesh / material component section as its own toggle
		auto* meshComp = entity.getComponent<MeshComponent>();
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		if (meshComp && ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::PopStyleVar();
			Material* material = meshComp->GetMaterial();
			const char* currentMatName = material ? material->name.c_str() : "<none>";

			ImGui::Text("Material");
			ImGui::SameLine();
			if (ImGui::BeginCombo("##Material", currentMatName)) {
				// Simple combo over all known materials
				const auto& allMaterials = MaterialManager::getAllMaterials();
				for (const auto& kv : allMaterials) {
					const std::string& matName = kv.first;
					bool isSelected = (material && matName == material->name);
					if (ImGui::Selectable(matName.c_str(), isSelected)) {
						meshComp->SetMaterial(kv.second);
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (material) {
				ImGui::Text("Albedo Texture:");
				ImGui::SameLine();

				// Drop target area for texture files dragged from the AssetBrowser
				ImGui::Button("Drop texture here");
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_FILE")) {
						const char* droppedPath = static_cast<const char*>(payload->Data);
						if (droppedPath && droppedPath[0] != '\0') {
							std::filesystem::path p(droppedPath);
							// Only allow common image formats
							std::string ext = p.extension().string();
							for (auto& c : ext) c = static_cast<char>(::tolower(c));
							if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") {
								std::string stem = p.stem().string();

								Texture* tex = nullptr;
								try {
									tex = TextureManager::getTexture(stem);
								}
								catch (...) {
									Texture* loaded = TextureManager::loadTexture(
										droppedPath, VulkanCore::getDevice(),
										VulkanCore::getPhysicalDevice(),
										VulkanCore::getCommandPool(),
										VulkanCore::getGraphicsQueue());
									TextureManager::createTextureImageView(loaded);
									TextureManager::createTextureSampler(loaded);
									TextureManager::registerTexture(stem, *loaded);
									tex = TextureManager::getTexture(stem);
								}

								Material* mat = MaterialManager::getMaterial(stem);
								if (!mat) {
									mat = MaterialManager::createMaterial(stem, stem);
								}

								meshComp->SetMaterial(mat);
							}
						}
					}
					ImGui::EndDragDropTarget();
				}
			}
		}
		else {
			ImGui::PopStyleVar();
		}
	}
	else if (selection.type == InspectorSelectionType::Asset &&
		!selection.assetPath.empty()) {
		const std::string& fullPath = selection.assetPath;
		std::filesystem::path p(fullPath);

		std::string fileName = p.filename().string();

		ImGui::BeginGroup();
		// Icon + name on the same line
		const FileEntry fe{fileName, fullPath, std::filesystem::is_directory(p)};
		const FileIcon& icon = AssetBrowser::GetIconForEntry(fe);
		if (icon.imguiTexture != VK_NULL_HANDLE) {
			ImGui::Image(reinterpret_cast<ImTextureID>(icon.imguiTexture), ImVec2(48.0f, 48.0f));
			ImGui::SameLine();
		}
		ImGui::TextUnformatted(fileName.c_str());
		ImGui::EndGroup();
	}

	ImGui::End();
}
