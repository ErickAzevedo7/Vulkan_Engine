#include "InspectorUi.h"

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <memory>
#include <managers/SceneManager.h>
#include <algorithm>
#include <string.h>
#include <string>
#include <unordered_map>

#include "components/CameraComponent.h"
#include "components/ColliderComponent.h"
#include "components/LightComponent.h"
#include "components/MeshComponent.h"
#include "components/ScriptComponent.h"
#include "components/StaticMeshColliderComponent.h"
#include "components/Transform.h"
#include "context/ResourceContext.h"
#include "core/vulkancore.h"
#include "Entity.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_internal.h"
#include "managers/MaterialManager.h"
#include "managers/PrefabSerializer.h"
#include "managers/ScriptCompiler.h"
#include "managers/ScriptPluginLoader.h"
#include "managers/ScriptRegistry.h"
#include "managers/TextureManager.h"
#include "renderer/vulkan/VulkanTexture.h"
#include "Scene.h"
#include "ui/AssetBrowser.h"
#include "vulkan/vulkan_core.h"

#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/trigonometric.hpp"


// Initialize constants
// Constants are initialized in the constructor/header

InspectorUi::InspectorUi(ResourceContext& resources)
	: resources(resources), kContentIndent(12.0f), kContentSpacing(0.0f, 6.0f) {
	selection = {};
	pickTarget = InspectorPickTarget::None;
}

InspectorUi::~InspectorUi() {
	releaseImGuiTextureSets();
}

void InspectorUi::releaseImGuiTextureSets() {
	imguiTextureSets.clear();
	imguiTextureCache.clear();
}

static bool Inspector_GetHeaderOpen(ImGuiID id, bool default_open) {
	ImGuiStorage* storage = ImGui::GetStateStorage();
	return storage->GetBool(id, default_open);
}

static void Inspector_SetHeaderOpen(ImGuiID id, bool is_open) {
	ImGuiStorage* storage = ImGui::GetStateStorage();
	storage->SetBool(id, is_open);
}

std::string InspectorUi::getMaterialNameFromPath(const std::string& fullPath) {
	std::filesystem::path p(fullPath);
	return p.stem().string();
}

void InspectorUi::resetPrefabEditingState() {
	prefabEditRoot = nullptr;
	prefabEditScene.reset();
	loadedPrefabPath.clear();
}

bool InspectorUi::ensurePrefabEditingLoaded(const std::string& prefabPath) {
	if (prefabEditScene && prefabEditRoot && loadedPrefabPath == prefabPath) {
		return true;
	}

	resetPrefabEditingState();
	prefabEditScene = std::make_unique<Scene>("PrefabInspector");
	prefabEditRoot = PrefabSerializer::instantiate(prefabPath, prefabEditScene.get(), resources);
	if (!prefabEditRoot) {
		resetPrefabEditingState();
		return false;
	}

	auto* entities = prefabEditScene->getEntities();
	if (entities) {
		for (const auto& ePtr : *entities) {
			ePtr->isSelected = false;
		}
	}
	prefabEditRoot->isSelected = true;

	loadedPrefabPath = prefabPath;
	return true;
}

void InspectorUi::selectEntity(int entityId) {
	resetPrefabEditingState();
	selection.type = InspectorSelectionType::Entity;
	selection.entityId = entityId;
	// Clear any asset selection state when an entity is picked
	selection.assetPath.clear();

	// Update per-entity selection flags so outline/rendering can use isSelected
	Scene* scene = resources.getSceneManager().getActiveScene();
	if (!scene)
		return;

	auto* entities = scene->getEntities();
	if (!entities)
		return;

	for (const auto& ePtr : *entities) {
		Entity& e = *ePtr;
		e.isSelected = (static_cast<int>(e.getID()) == entityId);
	}
}

void InspectorUi::selectAsset(const std::string& assetPath) {
	// Decide selection type based on asset extension
	std::filesystem::path p(assetPath);
	std::string ext = p.extension().string();
	for (auto& c : ext) {
		c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
	}

	if (ext == ".mat") {
		selection.type = InspectorSelectionType::Material;
		resetPrefabEditingState();
	} else if (ext == ".prefab") {
		selection.type = InspectorSelectionType::Prefab;
	} else {
		selection.type = InspectorSelectionType::Asset;
		resetPrefabEditingState();
	}

	selection.entityId = 0;
	selection.assetPath = assetPath;

	// Clear entity selection flags when switching to an asset selection
	Scene* scene = resources.getSceneManager().getActiveScene();
	if (!scene)
		return;

	auto* entities = scene->getEntities();
	if (!entities)
		return;

	for (const auto& ePtr : *entities) {
		ePtr->isSelected = false;
	}

	// If we are currently picking something for the inspector, apply it now.
	if (pickTarget != InspectorPickTarget::None) {
		const int selectedId = getSelectedEntityId();
		if (selectedId > 0 && scene) {
			Entity* entityPtr = nullptr;
			auto* entities = scene->getEntities();
			if (entities) {
				for (const auto& ePtr : *entities) {
					if (static_cast<int>(ePtr->getID()) == selectedId) {
						entityPtr = ePtr.get();
						break;
					}
				}
			}

			if (entityPtr && pickTarget == InspectorPickTarget::MeshAlbedo) {
				Entity& entity = *entityPtr;
				auto* meshComp = entity.getComponent<MeshComponent>();
				if (meshComp) {
					// Derive a logical name for the texture/material from the asset path
					std::filesystem::path p(assetPath);
					std::string stem = p.stem().string();
					std::string texKey = p.string(); // use full path as texture key

					// Ensure texture is loaded/registered.
					try {
						resources.getTextureManager().getTexture(texKey);
					} catch (...) {
					}

					// Create or reuse a material that uses this texture.
					// Use the texture file path as the material key.
					Material* mat = resources.getMaterialManager().getMaterial(texKey);
					if (!mat) {
						// Create material with file path key equal to texKey.
						mat = resources.getMaterialManager().createMaterial(stem, texKey, texKey);
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
	resetPrefabEditingState();
	selection.type = InspectorSelectionType::None;
	selection.entityId = 0;
	selection.assetPath.clear();

	// Clear all per-entity selection flags
	Scene* scene = resources.getSceneManager().getActiveScene();
	if (!scene)
		return;

	auto* entities = scene->getEntities();
	if (!entities)
		return;

	for (const auto& ePtr : *entities) {
		ePtr->isSelected = false;
	}
}

int InspectorUi::getSelectedEntityId() {
	if (selection.type == InspectorSelectionType::Entity) {
		return selection.entityId;
	}
	if (selection.type == InspectorSelectionType::Prefab && prefabEditRoot) {
		return static_cast<int>(prefabEditRoot->getID());
	}
	return -1;
}

const std::string& InspectorUi::getSelectedAssetPath() {
	return selection.assetPath;
}

bool InspectorUi::isEditingPrefab() const {
	return selection.type == InspectorSelectionType::Prefab &&
		!selection.assetPath.empty() &&
		prefabEditScene != nullptr &&
		prefabEditRoot != nullptr;
}

Scene* InspectorUi::getSceneOverrideForViewport() {
	if (selection.type == InspectorSelectionType::Prefab && !selection.assetPath.empty()) {
		if (ensurePrefabEditingLoaded(selection.assetPath)) {
			return prefabEditScene.get();
		}
	}
	return nullptr;
}

VkDescriptorSet InspectorUi::getOrCreateImGuiTextureSet(Texture* texture) {
	if (!texture)
		return VK_NULL_HANDLE;

	// Check cache first
	auto it = imguiTextureCache.find(texture);
	if (it != imguiTextureCache.end())
		return it->second;

	// Valid handle check
	if (!texture->handle.isValid() || !texture->sampler.isValid())
		return VK_NULL_HANDLE;

	VkDescriptorSet set = VK_NULL_HANDLE;
	auto* tm = &resources.getTextureManager();
	if (auto* vt = static_cast<Renderer::VulkanTexture*>(tm->getGraphicsTexture())) {
		VkSampler sampler = vt->getSampler(texture->sampler);
		VkImageView view = vt->getImageView(texture->handle);
		if (sampler != VK_NULL_HANDLE && view != VK_NULL_HANDLE) {
			set = ImGui_ImplVulkan_AddTexture(sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	}

	if (set != VK_NULL_HANDLE) {
		imguiTextureCache.emplace(texture, set);
	}
	return set;
}

bool InspectorUi::drawIconCollapsingHeader(const char* id,
										   ImTextureID iconTex,
										   const char* label,
										   ImGuiTreeNodeFlags flags) {
	flags |= ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;

	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImVec2 cursor = ImGui::GetCursorScreenPos();

	float fullWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
	float iconSizePx = iconTex ? 48.0f : ImGui::GetFrameHeight();
	float headerHeight = ImMax(ImGui::GetFrameHeight(), iconSizePx);
	ImVec2 headerSize(fullWidth, headerHeight);
	ImRect headerRect(cursor, ImVec2(cursor.x + headerSize.x, cursor.y + headerSize.y));

	ImGuiID headerId = window->GetID(id);
	// Manage open/close state ourselves using storage
	bool default_open = (flags & ImGuiTreeNodeFlags_DefaultOpen) != 0;
	bool isOpen = Inspector_GetHeaderOpen(headerId, default_open);

	// React to click on the whole header rectangle
	ImGui::ItemSize(headerRect);
	if (ImGui::ItemAdd(headerRect, headerId)) {
		bool hovered, held;
		bool pressed = ImGui::ButtonBehavior(headerRect, headerId, &hovered, &held);
		if (pressed) {
			isOpen = !isOpen;
			Inspector_SetHeaderOpen(headerId, isOpen);
		}
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImU32 headerCol = ImGui::GetColorU32(ImGui::IsItemHovered() ? ImGuiCol_HeaderHovered : ImGuiCol_Header);
	drawList->AddRectFilled(headerRect.Min, headerRect.Max, headerCol, 0.0f);

	// Draw a small triangle arrow on the left to signal open/closed state
	const ImGuiStyle& style = ImGui::GetStyle();
	const float arrowScale = 0.70f;
	const float arrowSize = ImGui::GetFontSize() * arrowScale;
	// Arrow position: left padding, vertically centered
	ImVec2 arrowPos(headerRect.Min.x + style.FramePadding.x, headerRect.Min.y + (headerSize.y - arrowSize) * 0.5f);
	ImRect arrowRect(arrowPos, ImVec2(arrowPos.x + arrowSize, arrowPos.y + arrowSize));
	ImGuiID arrowId = window->GetID((std::string(id) + "##arrow").c_str());
	if (ImGui::ItemAdd(arrowRect, arrowId)) {
		bool arrowHovered, arrowHeld;
		bool arrowPressed = ImGui::ButtonBehavior(arrowRect, arrowId, &arrowHovered, &arrowHeld);
		if (arrowPressed) {
			isOpen = !isOpen;
			Inspector_SetHeaderOpen(headerId, isOpen);
		}
	}
	ImU32 arrowCol = ImGui::GetColorU32(ImGuiCol_Text);
	ImGui::RenderArrow(drawList, arrowPos, arrowCol, isOpen ? ImGuiDir_Down : ImGuiDir_Right);

	ImVec2 textPos = cursor;

	float tree_spacing = style.ItemInnerSpacing.x * 0.5f;

	textPos.x += style.FramePadding.x + arrowSize + tree_spacing;
	if (iconTex) {
		ImVec2 iconSize(iconSizePx, iconSizePx);
		ImVec2 iconPos(textPos.x, cursor.y + (headerSize.y - iconSize.y) * 0.5f);
		drawList->AddImage(iconTex, iconPos, ImVec2(iconPos.x + iconSize.x, iconPos.y + iconSize.y));
		textPos.x = iconPos.x + iconSize.x + tree_spacing;
	} else {
		textPos.x = cursor.x + tree_spacing;
	}

	float textTopPadding = ImGui::GetStyle().FramePadding.y;
	textPos.y = cursor.y + textTopPadding;
	drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), label);

	// Move cursor below header for child content
	ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + headerSize.y));
	return isOpen;
}

void InspectorUi::renderMaterialTab(std::string fullPath) {
	ImGui::Indent(kContentIndent);
	ImGui::TextUnformatted("Material");
	ImGui::Spacing();

	std::string normFullPath;
	normFullPath = std::filesystem::path(fullPath).generic_string();
	const std::string defaultMaterialKey = "common/material/default.mat";
	const bool isDefaultMaterial =
		normFullPath == defaultMaterialKey ||
		(normFullPath.size() > defaultMaterialKey.size() &&
		 normFullPath.compare(normFullPath.size() - defaultMaterialKey.size(),
						  defaultMaterialKey.size(),
						  defaultMaterialKey) == 0);

	Material* material = resources.getMaterialManager().getMaterial(normFullPath);
	if (!material) {
		material = resources.getMaterialManager().loadMaterialFromFile(normFullPath);
	}
	if (!material) {
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Failed to load material.");
	} else {
		ImGui::Spacing();
		if (isDefaultMaterial) {
			ImGui::TextDisabled("Default material is read-only.");
			ImGui::Spacing();
		}

		auto isSupportedTextureExt = [](const std::string& ext) {
			return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" ||
				   ext == ".hdr";
		};

		auto drawTextureSlot = [&](const char* slotId,
								 const char* label,
								 const std::string& textureKey,
								 const std::function<Material*(const std::string&, const std::string&)>& updateFn) {
			ImVec2 labelSize = ImGui::CalcTextSize(label);
			ImVec2 squareSize(labelSize.y, labelSize.y);
			ImGui::InvisibleButton(slotId, squareSize);
			bool isHoveredTex = ImGui::IsItemHovered();
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 min = ImGui::GetItemRectMin();
			ImVec2 max = ImGui::GetItemRectMax();
			ImU32 borderCol = ImGui::GetColorU32(isHoveredTex ? ImGuiCol_ButtonHovered : ImGuiCol_Border);
			dl->AddRect(min, max, borderCol, 3.0f);

			Texture* previewTex = nullptr;
			try {
				previewTex = resources.getTextureManager().getTexture(textureKey);
			} catch (...) {
				previewTex = nullptr;
			}

			if (previewTex && previewTex->handle.isValid() && previewTex->sampler.isValid()) {
				ImVec2 innerMin(min.x + 2.0f, min.y + 2.0f);
				ImVec2 innerMax(max.x - 2.0f, max.y - 2.0f);
				VkDescriptorSet texSet = getOrCreateImGuiTextureSet(previewTex);
				if (texSet != VK_NULL_HANDLE) {
					dl->AddImage(reinterpret_cast<ImTextureID>(texSet), innerMin, innerMax);
				}
			} else {
				float inset = 2.0f;
				ImVec2 innerMin(min.x + inset, min.y + inset);
				ImVec2 innerMax(max.x - inset, max.y - inset);
				ImVec4 shadowBase = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
				shadowBase.w *= 0.9f;
				ImU32 shadowCol = ImGui::ColorConvertFloat4ToU32(shadowBase);
				dl->AddRectFilled(innerMin, innerMax, shadowCol, 2.0f);
			}

			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_FILE")) {
					const char* droppedPath = static_cast<const char*>(payload->Data);
					if (droppedPath && droppedPath[0] != '\0') {
						std::filesystem::path texPath(droppedPath);
						std::string texExt = texPath.extension().string();
						for (auto& c : texExt) {
							c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
						}

						if (isSupportedTextureExt(texExt)) {
							std::string texKey = texPath.generic_string();

							try {
								resources.getTextureManager().getTexture(texKey);
							} catch (...) {
							}

							std::string normPath = std::filesystem::path(fullPath).generic_string();
							Material* mat = updateFn(normPath, texKey);
							if (mat) {
								std::string matName = getMaterialNameFromPath(fullPath);
								resources.getMaterialManager().saveMaterialToFile(fullPath,
																	  matName,
																	  mat->albedoTextureKey,
																	  mat->roughnessTextureKey,
																	  mat->metallicTextureKey,
																	  mat->properties.albedo_pad,
																	  mat->properties.metallic,
																	  mat->properties.roughness,
																	  mat->properties.ao);
								if (Scene* scene = resources.getSceneManager().getActiveScene()) {
									scene->markDirty();
								}
							}
						}
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::SameLine();
			ImGui::TextUnformatted(label);
		};

		ImGui::BeginDisabled(isDefaultMaterial);

		drawTextureSlot("AlbedoTextureDropTarget",
						"Albedo",
						material->albedoTextureKey,
						[&](const std::string& materialPath, const std::string& texturePath) {
							return resources.getMaterialManager().updateMaterialTexture(materialPath, texturePath);
						});

		drawTextureSlot("RoughnessTextureDropTarget",
						"Roughness",
						material->roughnessTextureKey,
						[&](const std::string& materialPath, const std::string& texturePath) {
							return resources.getMaterialManager().updateMaterialRoughnessTexture(materialPath, texturePath);
						});

		drawTextureSlot("MetallicTextureDropTarget",
						"Metallic",
						material->metallicTextureKey,
						[&](const std::string& materialPath, const std::string& texturePath) {
							return resources.getMaterialManager().updateMaterialMetallicTexture(materialPath, texturePath);
						});

		// Material Properties section - keep same indentation as above
		ImGui::Spacing();
		ImGui::Spacing();

		// Use bold font for title if available (Fonts[1]), otherwise use scaled default
		ImGuiIO& io = ImGui::GetIO();

		ImGui::PushFont(io.Fonts->Fonts[1]); // Use bold font (index 1)
		ImGui::TextUnformatted("PBR Properties");
		ImGui::PopFont();

		ImGui::Spacing();
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kContentSpacing);

		// Use a unique ID for these columns to avoid state conflicts
		ImGui::Columns(2, "##MaterialPropsColumns", false);
		ImGui::SetColumnWidth(0, 60.0f);

		bool propertiesChanged = false;

		// Albedo
		ImGui::Text("Albedo");
		ImGui::NextColumn();
		if (ImGui::ColorEdit3("##Albedo",
							  &material->properties.albedo_pad.x,
							  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_Float)) {
			propertiesChanged = true;
		}
		ImGui::NextColumn();

		// Metallic
		ImGui::Text("Metallic");
		ImGui::NextColumn();
		if (ImGui::SliderFloat("##Metallic", &material->properties.metallic, 0.0f, 1.0f, "%.2f")) {
			propertiesChanged = true;
		}
		ImGui::NextColumn();

		// Roughness
		ImGui::Text("Roughness");
		ImGui::NextColumn();
		if (ImGui::SliderFloat("##Roughness", &material->properties.roughness, 0.0f, 1.0f, "%.2f")) {
			propertiesChanged = true;
		}
		ImGui::NextColumn();

		// AO
		ImGui::Text("AO");
		ImGui::NextColumn();
		if (ImGui::SliderFloat("##AO", &material->properties.ao, 0.0f, 1.0f, "%.2f")) {
			propertiesChanged = true;
		}
		ImGui::NextColumn();

		// Important: End columns before continuing
		ImGui::Columns(1);
		ImGui::PopStyleVar();

		// If properties changed, update GPU buffers and save to file
		if (propertiesChanged) {
			// Update all frames
			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				resources.getMaterialManager().updateMaterialProperties(material, static_cast<uint32_t>(i));
			}

			// Save to file
			std::string matName = getMaterialNameFromPath(fullPath);
			resources.getMaterialManager().saveMaterialToFile(fullPath,
											  matName,
											  material->albedoTextureKey,
											  material->roughnessTextureKey,
											  material->metallicTextureKey,
											  material->properties.albedo_pad,
											  material->properties.metallic,
											  material->properties.roughness,
															  material->properties.ao);

			if (Scene* scene = resources.getSceneManager().getActiveScene()) {
				scene->markDirty();
			}
		}

		ImGui::EndDisabled();
	}

	ImGui::Unindent(kContentIndent);
}

void InspectorUi::render() {
	Scene* activeScene = resources.getSceneManager().getActiveScene();
	bool edited = false;

	ImGui::PushStyleVarX(ImGuiStyleVar_WindowPadding, 0.0f);

	ImGui::Begin("Inspector");

	const float kHeaderContentTopPadding = 6.0f;
	Scene* inspectorScene = nullptr;
	Entity* entityPtr = nullptr;
	const bool isPrefabSelection =
		(selection.type == InspectorSelectionType::Prefab && !selection.assetPath.empty());

	if (selection.type == InspectorSelectionType::Entity && activeScene && selection.entityId > 0) {
		inspectorScene = activeScene;
		auto* entities = activeScene->getEntities();
		if (entities) {
			for (const auto& ePtr : *entities) {
				if (static_cast<int>(ePtr->getID()) == selection.entityId) {
					entityPtr = ePtr.get();
					break;
				}
			}
		}

		if (!entityPtr) {
			// Selected entity id not found in scene; clear selection
			clearSelection();
			ImGui::PopStyleVar();
			ImGui::End();
			return;
		}
	} else if (isPrefabSelection) {
		if (ensurePrefabEditingLoaded(selection.assetPath)) {
			inspectorScene = prefabEditScene.get();
			entityPtr = prefabEditRoot;
		}
	}

	if (entityPtr) {

		Entity& entity = *entityPtr;
		bool removeMeshComponent = false;
		bool removeLightComponent = false;
		bool removeCameraComponent = false;
		bool removeColliderComponent = false;
		bool removeStaticMeshColliderComponent = false;
		ScriptComponent* scriptToRemove = nullptr;

		auto drawComponentMenu =
			[&](const char* popupId, bool& removeFlag, const ImVec2& headerMin, const ImVec2& headerMax) {
				const float headerHeight = headerMax.y - headerMin.y;
				const float buttonSize = ImMax(16.0f, headerHeight - 4.0f);
				const ImVec2 buttonPos(headerMax.x - buttonSize - 6.0f,
									   headerMin.y + (headerHeight - buttonSize) * 0.5f);

				ImGui::SetCursorScreenPos(buttonPos);
				std::string buttonId = std::string("##btn_") + popupId;
				if (ImGui::InvisibleButton(buttonId.c_str(), ImVec2(buttonSize, buttonSize))) {
					ImGui::OpenPopup(popupId);
				}

				ImDrawList* dl = ImGui::GetWindowDrawList();
				const ImVec2 c = ImGui::GetItemRectMin();
				const ImVec2 d = ImGui::GetItemRectMax();
				if (ImGui::IsItemHovered()) {
					ImU32 hoverBg = ImGui::GetColorU32(ImGuiCol_FrameBgHovered);
					dl->AddRectFilled(c, d, hoverBg, 3.0f);
				}
				const float cx = (c.x + d.x) * 0.5f;
				const float cy = (c.y + d.y) * 0.5f;
				const float r = 1.6f;
				const float gap = 4.2f;
				const ImU32 dotCol = ImGui::GetColorU32(ImGuiCol_Text);
				dl->AddCircleFilled(ImVec2(cx, cy - gap), r, dotCol);
				dl->AddCircleFilled(ImVec2(cx, cy), r, dotCol);
				dl->AddCircleFilled(ImVec2(cx, cy + gap), r, dotCol);

				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
				ImGui::SetNextWindowPos(ImVec2(c.x, d.y + 2.0f), ImGuiCond_Appearing);
				if (ImGui::BeginPopup(popupId)) {
					if (ImGui::MenuItem("Remove Component")) {
						removeFlag = true;
						edited = true;
					}
					ImGui::EndPopup();
				}
				ImGui::PopStyleVar(2);
			};

		char nameBuffer[256];
		strncpy_s(nameBuffer, entity.getName().c_str(), sizeof(nameBuffer));
		nameBuffer[sizeof(nameBuffer) - 1] = 0;

		// Name row with padding
		ImGui::Indent(kContentIndent);
		ImGui::Text("Name");
		ImGui::SameLine();
		if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer))) {
			entity.setName(nameBuffer);
			edited = true;
		}

		ImGui::Unindent(kContentIndent);

		// Transform section
		auto* transform = entity.getComponent<Transform>();
		auto* meshComp = entity.getComponent<MeshComponent>();
		auto* lightComp = entity.getComponent<LightComponent>();
		auto* cameraComp = entity.getComponent<CameraComponent>();
		auto* colliderComp = entity.getComponent<ColliderComponent>();
		auto* staticMeshColliderComp = entity.getComponent<StaticMeshColliderComponent>();
		auto scriptComponents = entity.getComponents<ScriptComponent>();
		const bool hasScripts = !scriptComponents.empty();
		bool transformOpen = false;
		// Make collapsing header more compact by reducing FramePadding vertically
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.0f);
		if (transform && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
			transformOpen = true;
			ImGui::PopStyleVar(3);

			ImGui::Dummy(ImVec2(0.0f, kHeaderContentTopPadding));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kContentSpacing);
			ImGui::Indent(kContentIndent);

			ImGui::Columns(2, nullptr, false);
			ImGui::SetColumnWidth(0, 60.0f);

			// Position
			ImGui::Text("Position");
			ImGui::NextColumn();
			if (ImGui::DragFloat3("##Position", &transform->position.x, 0.1f, 0, 0, "%.2f"))
				edited = true;
			ImGui::NextColumn();

			// Rotation
			ImGui::Text("Rotation");
			ImGui::NextColumn();
			glm::vec3 euler = glm::degrees(glm::eulerAngles(transform->rotation));
			if (ImGui::DragFloat3("##Rotation", &euler.x, 0.5f, 0, 0, "%.2f")) {
				transform->rotation = glm::quat(glm::radians(euler));
				edited = true;
			}
			ImGui::NextColumn();

			// Scale
			ImGui::Text("Scale");
			ImGui::NextColumn();
			if (ImGui::DragFloat3("##Scale", &transform->scale.x, 0.1f, 0, 0, "%.2f"))
				edited = true;
			ImGui::Columns(1);

			ImGui::Unindent(kContentIndent);
			ImGui::PopStyleVar();
		} else {
			ImGui::PopStyleVar(3);
		}

		if (meshComp || lightComp || cameraComp || colliderComp || staticMeshColliderComp || hasScripts) {
			ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 1.0f);
			ImGui::Separator();
			ImGui::PopStyleVar();
		}

		// Mesh / material section
		bool meshOpen = false;
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.0f);
		if (meshComp) {
			meshOpen =
				ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
			const ImVec2 meshHeaderMin = ImGui::GetItemRectMin();
			const ImVec2 meshHeaderMax = ImGui::GetItemRectMax();
			drawComponentMenu("MeshComponentMenu", removeMeshComponent, meshHeaderMin, meshHeaderMax);
			ImGui::PopStyleVar(3);

			if (meshOpen) {
				ImGui::Dummy(ImVec2(0.0f, kHeaderContentTopPadding));

				ImGui::Indent(kContentIndent);

				Material* material = meshComp->GetMaterial();
				const char* currentMatName = material ? material->name.c_str() : "<none>";

				ImGui::Text("Material");
				ImGui::SameLine();
				if (ImGui::BeginCombo("##Material", currentMatName)) {
					// Simple combo over all known materials
					const auto& allMaterials = resources.getMaterialManager().getAllMaterials();
					for (const auto& kv : allMaterials) {
						const std::string& matName = kv.first;
						bool isSelected = (material && matName == material->name);
						if (ImGui::Selectable(matName.c_str(), isSelected)) {
							meshComp->SetMaterial(kv.second);
							edited = true;
						}
						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				if (material) {
					// Square drop target for material files (.mat) dragged from the AssetBrowser
					const char* matLabel = "Material";
					ImVec2 labelSize = ImGui::CalcTextSize(matLabel);
					ImVec2 squareSize(labelSize.y, labelSize.y);
					ImGui::InvisibleButton("MaterialDropTarget", squareSize);
					bool isHovered = ImGui::IsItemHovered();

					ImDrawList* dl = ImGui::GetWindowDrawList();
					ImVec2 min = ImGui::GetItemRectMin();
					ImVec2 max = ImGui::GetItemRectMax();
					ImU32 borderCol = ImGui::GetColorU32(isHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Border);
					dl->AddRect(min, max, borderCol, 3.0f);

					// Preview the material's albedo texture (look up by key)
					Texture* previewTex = nullptr;
					try {
						previewTex = resources.getTextureManager().getTexture(material->albedoTextureKey);
					} catch (...) {
						previewTex = nullptr;
					}

					if (previewTex && previewTex->handle.isValid() && previewTex->sampler.isValid()) {
						ImVec2 innerMin(min.x + 2.0f, min.y + 2.0f);
						ImVec2 innerMax(max.x - 2.0f, max.y - 2.0f);

						// Resolve handle to VkDescriptorSet via getOrCreateImGuiTextureSet
						VkDescriptorSet texSet = getOrCreateImGuiTextureSet(previewTex);

						if (texSet != VK_NULL_HANDLE) {
							dl->AddImage(reinterpret_cast<ImTextureID>(texSet), innerMin, innerMax);
						}
					} else {
						// Fallback: simple inner shadow box
						float inset = 2.0f;
						ImVec2 innerMin(min.x + inset, min.y + inset);
						ImVec2 innerMax(max.x - inset, max.y - inset);
						ImVec4 shadowBase = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
						shadowBase.w *= 0.9f;
						ImU32 shadowCol = ImGui::ColorConvertFloat4ToU32(shadowBase);
						dl->AddRectFilled(innerMin, innerMax, shadowCol, 2.0f);
					}

					// Attach drag-drop target to the square
					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_FILE")) {
							const char* droppedPath = static_cast<const char*>(payload->Data);
							if (droppedPath && droppedPath[0] != '\0') {
								std::filesystem::path p(droppedPath);
								std::string ext = p.extension().string();
								for (auto& c : ext)
									c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));

								// Only accept material assets here
								if (ext == ".mat") {
									// Derive material name from .mat filename
									std::string matName = getMaterialNameFromPath(droppedPath);

									// Ensure material exists via MaterialManager
									Material* mat = resources.getMaterialManager().loadMaterialFromFile(droppedPath);
									if (!mat) {
										// Fallback: create with default albedo if file not valid yet
										mat = resources.getMaterialManager().createMaterial(matName, "default");
										resources.getMaterialManager().saveMaterialToFile(
											droppedPath, matName, "default");
									}

									meshComp->SetMaterial(mat);
									edited = true;
								}
							}
						}
						ImGui::EndDragDropTarget();
					}

					ImGui::SameLine();
					ImGui::TextUnformatted(matLabel);

					if (!material->filePath.empty()) {
						ImGui::Unindent(kContentIndent);

						ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 1.0f);
						ImGui::Separator();
						ImGui::PopStyleVar();

						// Get icon from AssetBrowser if available
						ImTextureID iconTex = 0;
						if (assetBrowser) {
							const FileEntry fe{material->name, material->filePath};
							const FileIcon& icon = assetBrowser->GetIconForEntry(fe);
							if (icon.imguiTexture != VK_NULL_HANDLE) {
								iconTex = reinterpret_cast<ImTextureID>(icon.imguiTexture);
							}
						}

						ImGuiTreeNodeFlags matFlags = ImGuiTreeNodeFlags_DefaultOpen;
						ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
						if (drawIconCollapsingHeader("MaterialHeader", iconTex, material->name.c_str(), matFlags)) {
							ImGui::Dummy(ImVec2(0.0f, 6.0f));
							renderMaterialTab(material->filePath);
						}
						ImGui::PopStyleVar();
					}
				}
			}
		} else {
			ImGui::PopStyleVar(3);
		}

		if (meshComp && (lightComp || cameraComp || hasScripts)) {
			ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 1.0f);
			ImGui::Separator();
			ImGui::PopStyleVar();
		}

		// Light component section
		bool lightOpen = false;
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.0f);
		if (lightComp) {
			lightOpen =
				ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
			const ImVec2 lightHeaderMin = ImGui::GetItemRectMin();
			const ImVec2 lightHeaderMax = ImGui::GetItemRectMax();
			drawComponentMenu("LightComponentMenu", removeLightComponent, lightHeaderMin, lightHeaderMax);
			ImGui::PopStyleVar(3);

			if (lightOpen) {
				ImGui::Dummy(ImVec2(0.0f, kHeaderContentTopPadding));
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kContentSpacing);
				ImGui::Indent(kContentIndent);

				ImGui::Columns(2, "##LightColumns", false);
				ImGui::SetColumnWidth(0, 80.0f);

				// Light Type
				ImGui::Text("Type");
				ImGui::NextColumn();
				const char* lightTypeLabels[] = {"Directional", "Point", "Spot"};
				int currentType = static_cast<int>(lightComp->getType());
				if (ImGui::Combo("##LightType", &currentType, lightTypeLabels, IM_ARRAYSIZE(lightTypeLabels))) {
					lightComp->setType(static_cast<LightType>(currentType));
					edited = true;
				}
				ImGui::NextColumn();

				// Color
				ImGui::Text("Color");
				ImGui::NextColumn();
				if (ImGui::ColorEdit3("##LightColor",
									  &lightComp->color.x,
									  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
					edited = true;
				ImGui::NextColumn();

				// Intensity
				ImGui::Text("Intensity");
				ImGui::NextColumn();
				if (ImGui::DragFloat("##LightIntensity", &lightComp->intensity, 1.0f, 0.0f, 1000.0f, "%.1f"))
					edited = true;
				ImGui::NextColumn();

				ImGui::Columns(1);

				ImGui::Columns(1);

				// Attenuation section (for point/spot lights)
				if (lightComp->getType() != LightType::Directional) {
					ImGui::Spacing();
					ImGui::Spacing();

					ImGuiIO& ioAtt = ImGui::GetIO();
					ImGui::PushFont(ioAtt.Fonts->Fonts[1]); // Bold font
					ImGui::TextUnformatted("Attenuation");
					ImGui::PopFont();

					ImGui::Spacing();

					ImGui::Columns(2, "##LightAttenColumns", false);
					ImGui::SetColumnWidth(0, 80.0f);

					// Constant (Kc)
					ImGui::Text("Kc (Const)");
					ImGui::NextColumn();
					if (ImGui::DragFloat("##LightAttenKc", &lightComp->attenuationKc, 0.01f, 0.0f, 100.0f, "%.3f"))
						edited = true;
					ImGui::NextColumn();

					// Linear (Kl)
					ImGui::Text("Kl (Linear)");
					ImGui::NextColumn();
					if (ImGui::DragFloat("##LightAttenKl", &lightComp->attenuationKl, 0.01f, 0.0f, 100.0f, "%.3f"))
						edited = true;
					ImGui::NextColumn();

					// Quadratic (Kq)
					ImGui::Text("Kq (Quad)");
					ImGui::NextColumn();
					if (ImGui::DragFloat("##LightAttenKq", &lightComp->attenuationKq, 0.001f, 0.0f, 100.0f, "%.4f"))
						edited = true;
					ImGui::NextColumn();

					ImGui::Columns(1);
				}

				// Spotlight cone angle section (only for spotlights)
				if (lightComp->getType() == LightType::Spot) {
					ImGui::Spacing();
					ImGui::Spacing();

					ImGuiIO& ioSpot = ImGui::GetIO();
					ImGui::PushFont(ioSpot.Fonts->Fonts[1]); // Bold font
					ImGui::TextUnformatted("Spotlight Cone");
					ImGui::PopFont();

					ImGui::Spacing();

					ImGui::Columns(2, "##SpotlightConeColumns", false);
					ImGui::SetColumnWidth(0, 80.0f);

					// Inner Cone Angle
					ImGui::Text("Inner Cone");
					ImGui::NextColumn();
					float innerDegrees = glm::degrees(lightComp->innerConeAngle);
					if (ImGui::SliderFloat("##InnerCone", &innerDegrees, 0.0f, 90.0f, "%.1f°")) {
						lightComp->innerConeAngle = glm::radians(innerDegrees);
						// Ensure inner cone is not larger than outer cone
						if (lightComp->innerConeAngle > lightComp->outerConeAngle) {
							lightComp->outerConeAngle = lightComp->innerConeAngle;
						}
						edited = true;
					}
					ImGui::NextColumn();

					// Outer Cone Angle
					ImGui::Text("Outer Cone");
					ImGui::NextColumn();
					float outerDegrees = glm::degrees(lightComp->outerConeAngle);
					if (ImGui::SliderFloat("##OuterCone", &outerDegrees, 0.0f, 90.0f, "%.1f°")) {
						lightComp->outerConeAngle = glm::radians(outerDegrees);
						// Ensure outer cone is not smaller than inner cone
						if (lightComp->outerConeAngle < lightComp->innerConeAngle) {
							lightComp->innerConeAngle = lightComp->outerConeAngle;
						}
						edited = true;
					}
					ImGui::NextColumn();
				}

				ImGui::Unindent(kContentIndent);
				ImGui::PopStyleVar();
			}
		} else {
			ImGui::PopStyleVar(3);
		}

		if (lightComp && (cameraComp || hasScripts)) {
			ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 1.0f);
			ImGui::Separator();
			ImGui::PopStyleVar();
		}

		// Camera component section
		bool cameraOpen = false;
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.0f);
		if (cameraComp) {
			cameraOpen =
				ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
			const ImVec2 cameraHeaderMin = ImGui::GetItemRectMin();
			const ImVec2 cameraHeaderMax = ImGui::GetItemRectMax();
			drawComponentMenu("CameraComponentMenu", removeCameraComponent, cameraHeaderMin, cameraHeaderMax);
			ImGui::PopStyleVar(3);

			if (cameraOpen) {
				ImGui::Dummy(ImVec2(0.0f, kHeaderContentTopPadding));
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kContentSpacing);
				ImGui::Indent(kContentIndent);

				ImGui::Columns(2, "##CameraColumns", false);
				ImGui::SetColumnWidth(0, 80.0f);

				// FOV
				ImGui::Text("FOV");
				ImGui::NextColumn();
				if (ImGui::SliderFloat("##CameraFOV", &cameraComp->fov, 10.0f, 150.0f, "%.1f"))
					edited = true;
				ImGui::NextColumn();

				// Near Plane
				ImGui::Text("Near Plane");
				ImGui::NextColumn();
				if (ImGui::DragFloat("##CameraNear", &cameraComp->nearPlane, 0.01f, 0.001f, 10.0f, "%.3f"))
					edited = true;
				ImGui::NextColumn();

				// Far Plane
				ImGui::Text("Far Plane");
				ImGui::NextColumn();
				if (ImGui::DragFloat("##CameraFar", &cameraComp->farPlane, 1.0f, 10.0f, 10000.0f, "%.1f"))
					edited = true;
				ImGui::NextColumn();

				// Is Primary
				ImGui::Text("Primary");
				ImGui::NextColumn();
				if (ImGui::Checkbox("##CameraPrimary", &cameraComp->isPrimary))
					edited = true;
				ImGui::NextColumn();

				ImGui::Columns(1);

				ImGui::Unindent(kContentIndent);
				ImGui::PopStyleVar();
			}
		} else {
			ImGui::PopStyleVar(3);
		}

		if (cameraComp && (colliderComp || staticMeshColliderComp || hasScripts)) {
			ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 1.0f);
			ImGui::Separator();
			ImGui::PopStyleVar();
		}

		bool colliderOpen = false;
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.0f);
		if (colliderComp) {
			colliderOpen =
				ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
			const ImVec2 colliderHeaderMin = ImGui::GetItemRectMin();
			const ImVec2 colliderHeaderMax = ImGui::GetItemRectMax();
			drawComponentMenu("ColliderComponentMenu", removeColliderComponent, colliderHeaderMin, colliderHeaderMax);
			ImGui::PopStyleVar(3);

			if (colliderOpen) {
				ImGui::Dummy(ImVec2(0.0f, kHeaderContentTopPadding));
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kContentSpacing);
				ImGui::Indent(kContentIndent);

				ImGui::Columns(2, "##ColliderColumns", false);
				ImGui::SetColumnWidth(0, 90.0f);

				ImGui::Text("Enabled");
				ImGui::NextColumn();
				if (ImGui::Checkbox("##ColliderEnabled", &colliderComp->enabled))
					edited = true;
				ImGui::NextColumn();

				ImGui::Text("Trigger");
				ImGui::NextColumn();
				if (ImGui::Checkbox("##ColliderTrigger", &colliderComp->isTrigger))
					edited = true;
				ImGui::NextColumn();

				ImGui::Text("Static");
				ImGui::NextColumn();
				if (ImGui::Checkbox("##ColliderStatic", &colliderComp->isStatic))
					edited = true;
				ImGui::NextColumn();

				ImGui::Text("Center");
				ImGui::NextColumn();
				if (ImGui::DragFloat3("##ColliderCenter", &colliderComp->center.x, 0.05f, 0.0f, 0.0f, "%.2f"))
					edited = true;
				ImGui::NextColumn();

				ImGui::Text("Size");
				ImGui::NextColumn();
				if (ImGui::DragFloat3("##ColliderSize", &colliderComp->size.x, 0.05f, 0.01f, 0.0f, "%.2f")) {
					colliderComp->size.x = (std::max)(colliderComp->size.x, 0.01f);
					colliderComp->size.y = (std::max)(colliderComp->size.y, 0.01f);
					colliderComp->size.z = (std::max)(colliderComp->size.z, 0.01f);
					edited = true;
				}
				ImGui::NextColumn();

				ImGui::Columns(1);

				ImGui::Unindent(kContentIndent);
				ImGui::PopStyleVar();
			}
		} else {
			ImGui::PopStyleVar(3);
		}

		if (colliderComp && (staticMeshColliderComp || hasScripts)) {
			ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 1.0f);
			ImGui::Separator();
			ImGui::PopStyleVar();
		}

		bool staticMeshColliderOpen = false;
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.0f);
		if (staticMeshColliderComp) {
			staticMeshColliderOpen = ImGui::CollapsingHeader("Static Mesh Collider",
				ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
			const ImVec2 staticMeshColliderHeaderMin = ImGui::GetItemRectMin();
			const ImVec2 staticMeshColliderHeaderMax = ImGui::GetItemRectMax();
			drawComponentMenu("StaticMeshColliderComponentMenu",
				removeStaticMeshColliderComponent,
				staticMeshColliderHeaderMin,
				staticMeshColliderHeaderMax);
			ImGui::PopStyleVar(3);

			if (staticMeshColliderOpen) {
				ImGui::Dummy(ImVec2(0.0f, kHeaderContentTopPadding));
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kContentSpacing);
				ImGui::Indent(kContentIndent);

				ImGui::Columns(2, "##StaticMeshColliderColumns", false);
				ImGui::SetColumnWidth(0, 120.0f);

				ImGui::Text("Enabled");
				ImGui::NextColumn();
				if (ImGui::Checkbox("##StaticMeshColliderEnabled", &staticMeshColliderComp->enabled))
					edited = true;
				ImGui::NextColumn();

				ImGui::Text("Trigger");
				ImGui::NextColumn();
				if (ImGui::Checkbox("##StaticMeshColliderTrigger", &staticMeshColliderComp->isTrigger))
					edited = true;
				ImGui::NextColumn();

				ImGui::Text("Use Mesh Bounds");
				ImGui::NextColumn();
				if (ImGui::Checkbox("##StaticMeshColliderUseMeshBounds", &staticMeshColliderComp->useAttachedMeshBounds))
					edited = true;
				ImGui::NextColumn();

				ImGui::Text("Local Center");
				ImGui::NextColumn();
				if (ImGui::DragFloat3("##StaticMeshColliderLocalCenter", &staticMeshColliderComp->localCenter.x, 0.05f, 0.0f, 0.0f, "%.2f"))
					edited = true;
				ImGui::NextColumn();

				ImGui::Text("Local Size");
				ImGui::NextColumn();
				if (ImGui::DragFloat3("##StaticMeshColliderLocalSize", &staticMeshColliderComp->localSize.x, 0.05f, 0.01f, 0.0f, "%.2f")) {
					staticMeshColliderComp->localSize.x = (std::max)(staticMeshColliderComp->localSize.x, 0.01f);
					staticMeshColliderComp->localSize.y = (std::max)(staticMeshColliderComp->localSize.y, 0.01f);
					staticMeshColliderComp->localSize.z = (std::max)(staticMeshColliderComp->localSize.z, 0.01f);
					edited = true;
				}
				ImGui::NextColumn();

				ImGui::Columns(1);

				ImGui::Unindent(kContentIndent);
				ImGui::PopStyleVar();
			}
		} else {
			ImGui::PopStyleVar(3);
		}

		if (staticMeshColliderComp && hasScripts) {
			ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 1.0f);
			ImGui::Separator();
			ImGui::PopStyleVar();
		}

		// ----------------------------------------------------------------
		// Script component section
		// ----------------------------------------------------------------
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.0f);
		if (hasScripts) {
			for (size_t i = 0; i < scriptComponents.size(); ++i) {
				ScriptComponent* scriptComp = scriptComponents[i];
				ImGui::PushID(scriptComp);

				std::string scriptHeaderLabel =
					scriptComp->scriptName.empty() ? "Script" : (scriptComp->scriptName + " (Script)");
				bool scriptOpen =
					ImGui::CollapsingHeader(scriptHeaderLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
				const ImVec2 scriptHeaderMin = ImGui::GetItemRectMin();
				const ImVec2 scriptHeaderMax = ImGui::GetItemRectMax();

				bool removeThisScript = false;
				std::string scriptPopupId = "ScriptComponentMenu_" + std::to_string(i);
				drawComponentMenu(scriptPopupId.c_str(), removeThisScript, scriptHeaderMin, scriptHeaderMax);
				if (removeThisScript) {
					scriptToRemove = scriptComp;
				}

				if (scriptOpen) {
					ImGui::Dummy(ImVec2(0.0f, kHeaderContentTopPadding));
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kContentSpacing);
					ImGui::Indent(kContentIndent);

					ImGui::Columns(2, "##ScriptColumns", false);
					ImGui::SetColumnWidth(0, 80.0f);

					ImGui::Text("Type");
					ImGui::NextColumn();
					ImGui::TextDisabled("%s", scriptComp->scriptName.c_str());
					ImGui::NextColumn();

					ImGui::Text("Enabled");
					ImGui::NextColumn();
					if (ImGui::Checkbox("##ScriptEnabled", &scriptComp->enabled))
						edited = true;
					ImGui::NextColumn();

					auto inspectorProps = scriptComp->getInspectorProperties();
					for (const auto& prop : inspectorProps) {
						ImGui::Text("%s", prop.name.c_str());
						ImGui::NextColumn();
						bool changed = false;
						switch (prop.type) {
						case InspectorPropertyType::Float: {
							float* val = std::get<float*>(prop.ptr);
							changed = ImGui::DragFloat(("##" + prop.name).c_str(), val, 0.1f);
							break;
						}
						case InspectorPropertyType::Int: {
							int* val = std::get<int*>(prop.ptr);
							changed = ImGui::DragInt(("##" + prop.name).c_str(), val, 1.0f);
							break;
						}
						case InspectorPropertyType::Bool: {
							bool* val = std::get<bool*>(prop.ptr);
							changed = ImGui::Checkbox(("##" + prop.name).c_str(), val);
							break;
						}
						case InspectorPropertyType::Vec3: {
							glm::vec3* val = std::get<glm::vec3*>(prop.ptr);
							changed = ImGui::DragFloat3(("##" + prop.name).c_str(), &val->x, 0.1f);
							break;
						}
						case InspectorPropertyType::Vec4: {
							glm::vec4* val = std::get<glm::vec4*>(prop.ptr);
							changed = ImGui::DragFloat4(("##" + prop.name).c_str(), &val->x, 0.1f);
							break;
						}
						}
						if (changed)
							edited = true;
						ImGui::NextColumn();
					}

					ImGui::Columns(1);

					ImGui::Unindent(kContentIndent);
					ImGui::PopStyleVar();
				}

				if (i + 1 < scriptComponents.size()) {
					ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 1.0f);
					ImGui::Separator();
					ImGui::PopStyleVar();
				}

				ImGui::PopID();
			}

			ImGui::PopStyleVar(3);
		} else {
			ImGui::PopStyleVar(3);
		}
		const char* compilePopupId = "##ScriptCompilePopup";
		if (ScriptCompiler::isCompiling()) {
			ImGui::OpenPopup(compilePopupId);
		}

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_Appearing);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
		if (ImGui::BeginPopupModal(compilePopupId,
								   nullptr,
								   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove)) {
			ImGui::TextUnformatted("Compiling script...");
			const std::string compilingName = ScriptCompiler::compilingScriptName();
			if (!compilingName.empty()) {
				ImGui::TextDisabled("%s", compilingName.c_str());
			}

			ImGui::Dummy(ImVec2(0.0f, 4.0f));
			const ImVec2 barSize(ImGui::GetContentRegionAvail().x, 14.0f);
			ImGui::InvisibleButton("##CompileActivityBar", barSize);
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImRect barRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
			ImU32 barBg = ImGui::GetColorU32(ImGuiCol_FrameBg);
			ImU32 barFill = IM_COL32(86, 196, 118, 235);
			ImU32 barBorder = ImGui::GetColorU32(ImGuiCol_Border);
			dl->AddRectFilled(barRect.Min, barRect.Max, barBg, 4.0f);
			const float t = static_cast<float>(ImGui::GetTime());
			const float phase = fmodf(t * 0.9f, 1.0f);
			const float segmentWidth = barSize.x * 0.35f;
			const float segmentStart = barRect.Min.x + phase * (barSize.x + segmentWidth) - segmentWidth;
			const float segmentEnd = segmentStart + segmentWidth;
			const float clampedStart = ImMax(segmentStart, barRect.Min.x);
			const float clampedEnd = ImMin(segmentEnd, barRect.Max.x);
			if (clampedEnd > clampedStart) {
				dl->AddRectFilled(ImVec2(clampedStart, barRect.Min.y), ImVec2(clampedEnd, barRect.Max.y), barFill, 4.0f);
			}
			dl->AddRect(barRect.Min, barRect.Max, barBorder, 4.0f);

			if (!ScriptCompiler::isCompiling()) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(2);

		auto attachScriptByName = [&](const std::string& scriptName, const std::string& headerPath) {
			ScriptComponent* sc = ScriptRegistry::create(scriptName);
			if (sc) {
				sc->headerPath = headerPath;
				entity.addComponent(sc);
				edited = true;
				if (inspectorScene) {
					inspectorScene->markDirty();
				}
			}
		};

		ImRect dropRect(ImGui::GetWindowPos(),
						ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x,
							   ImGui::GetWindowPos().y + ImGui::GetWindowSize().y));
		if (ImGui::BeginDragDropTargetCustom(dropRect, ImGui::GetID("##InspectorEntityDropTarget"))) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_FILE")) {
				std::string filePath(static_cast<const char*>(payload->Data), payload->DataSize - 1);
				std::filesystem::path p(filePath);
				std::string ext = p.extension().string();
				for (auto& c : ext)
					c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));

				if (ext == ".dll") {
					ScriptPluginLoader::loadPlugin(filePath);
				} else if (ext == ".h") {
					std::string scriptName = p.stem().string();

					auto registered = ScriptRegistry::getRegisteredNames();
					bool alreadyRegistered = false;
					for (const auto& r : registered) {
						if (r == scriptName) {
							alreadyRegistered = true;
							break;
						}
					}

					if (alreadyRegistered) {
						attachScriptByName(scriptName, filePath);
					} else {
						uint32_t targetEntityId = entity.getID();
						const bool targetIsPrefab = (selection.type == InspectorSelectionType::Prefab);
						const std::string targetPrefabPath = loadedPrefabPath;
						ScriptCompiler::compileAsync(
							filePath,
							[this, targetEntityId, scriptName, filePath, targetIsPrefab, targetPrefabPath](bool ok,
																								 const std::string& dllPath) {
								if (!ok) {
									std::cerr << "[Inspector] Script compile failed. Check build logs.\n";
									return;
								}

								ScriptPluginLoader::loadPlugin(dllPath);

								Scene* targetScene = nullptr;
								if (targetIsPrefab) {
									if (!(prefabEditScene && loadedPrefabPath == targetPrefabPath)) {
										return;
									}
									targetScene = prefabEditScene.get();
								} else {
									targetScene = resources.getSceneManager().getActiveScene();
								}

								if (!targetScene)
									return;

								Entity* target = targetScene->findEntityById(targetEntityId);
								if (!target)
									return;

							ScriptComponent* sc = ScriptRegistry::create(scriptName);
							if (sc) {
								sc->headerPath = filePath;
									target->addComponent(sc);
									targetScene->markDirty();
								}
							});
					}
				}
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 1.0f);
		ImGui::Separator();
		ImGui::PopStyleVar();
		ImGui::Dummy(ImVec2(0.0f, 8.0f));

		const char* addComponentLabel = "Add Component";
		const float addButtonWidth = 200.0f;
		const float availableWidth = ImGui::GetContentRegionAvail().x;
		if (availableWidth > addButtonWidth) {
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availableWidth - addButtonWidth) * 0.5f);
		}

		if (ImGui::Button(addComponentLabel, ImVec2(addButtonWidth, 0.0f))) {
			ImGui::OpenPopup("##AddComponentPopup");
		}

		const ImVec2 addBtnMin = ImGui::GetItemRectMin();
		const ImVec2 addBtnMax = ImGui::GetItemRectMax();
		const float popupWidth = addBtnMax.x - addBtnMin.x;
		ImGui::SetNextWindowPos(ImVec2(addBtnMin.x, addBtnMax.y + 2.0f), ImGuiCond_Appearing);
		ImGui::SetNextWindowSizeConstraints(ImVec2(popupWidth, 0.0f), ImVec2(popupWidth, 10000.0f));
		ImGui::SetNextWindowSize(ImVec2(popupWidth, 0.0f), ImGuiCond_Always);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
		if (ImGui::BeginPopup("##AddComponentPopup")) {
			if (!entity.hasComponent<MeshComponent>() && ImGui::MenuItem("Mesh")) {
				MeshComponent* meshComp = new MeshComponent(&entity, "cube", resources.getMeshManager());
				if (auto* mat = resources.getMaterialManager().getMaterial("common/material/default.mat")) {
					meshComp->SetMaterial(mat);
				}
				entity.addComponent(meshComp);
				edited = true;
			}

			if (!entity.hasComponent<LightComponent>() && ImGui::MenuItem("Light")) {
				entity.addComponent(new LightComponent(&entity, LightType::Point));
				edited = true;
			}

			if (!entity.hasComponent<CameraComponent>() && ImGui::MenuItem("Camera")) {
				entity.addComponent(new CameraComponent(&entity));
				edited = true;
			}

			if (!entity.hasComponent<ColliderComponent>() && ImGui::MenuItem("Collider")) {
				entity.addComponent(new ColliderComponent());
				edited = true;
			}

			if (!entity.hasComponent<StaticMeshColliderComponent>() && ImGui::MenuItem("Static Mesh Collider")) {
				entity.addComponent(new StaticMeshColliderComponent());
				edited = true;
			}

			auto registeredScripts = ScriptRegistry::getRegisteredNames();
			if (registeredScripts.empty()) {
				ImGui::TextDisabled("Script (none registered)");
			} else if (ImGui::BeginMenu("Script")) {
				for (const auto& scriptName : registeredScripts) {
					if (ImGui::MenuItem(scriptName.c_str())) {
						if (ScriptComponent* sc = ScriptRegistry::create(scriptName)) {
							entity.addComponent(sc);
							edited = true;
						}
					}
				}
				ImGui::EndMenu();
			}

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(2);

		if (removeMeshComponent) {
			entity.removeComponent<MeshComponent>();
		}
		if (removeLightComponent) {
			entity.removeComponent<LightComponent>();
		}
		if (removeCameraComponent) {
			entity.removeComponent<CameraComponent>();
		}
		if (removeColliderComponent) {
			entity.removeComponent<ColliderComponent>();
		}
		if (removeStaticMeshColliderComponent) {
			entity.removeComponent<StaticMeshColliderComponent>();
		}
		if (scriptToRemove) {
			entity.removeComponent(scriptToRemove);
		}
	} else if ((selection.type == InspectorSelectionType::Asset ||
				selection.type == InspectorSelectionType::Material) &&
			   !selection.assetPath.empty()) {
		const std::string& fullPath = selection.assetPath;
		std::filesystem::path p(fullPath);
		std::string fileName = p.filename().string();

		ImGui::BeginGroup();
		ImTextureID iconTex = 0;
		if (assetBrowser) {
			const FileEntry fe{fileName, fullPath, std::filesystem::is_directory(p)};
			const FileIcon& icon = assetBrowser->GetIconForEntry(fe);
			if (icon.imguiTexture != VK_NULL_HANDLE) {
				iconTex = reinterpret_cast<ImTextureID>(icon.imguiTexture);
			}
		}

		if (iconTex != 0) {
			ImGui::Image(iconTex, ImVec2(48.0f, 48.0f));
			ImGui::SameLine();
		}
		ImGui::TextUnformatted(fileName.c_str());
		ImGui::EndGroup();

		if (selection.type == InspectorSelectionType::Material) {
			InspectorUi::renderMaterialTab(fullPath);
		}
	} else if (selection.type == InspectorSelectionType::Prefab && !selection.assetPath.empty()) {
		std::filesystem::path p(selection.assetPath);
		ImGui::TextUnformatted(p.filename().string().c_str());
		ImGui::TextDisabled("Failed to load prefab.");
	}

	if (edited) {
		if (selection.type == InspectorSelectionType::Prefab && prefabEditRoot && !selection.assetPath.empty()) {
			PrefabSerializer::save(selection.assetPath, *prefabEditRoot);
		} else if (inspectorScene) {
			inspectorScene->markDirty();
		}
	}

	ImGui::PopStyleVar();
	ImGui::End();
}
