#include "InspectorUi.h"

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <managers/SceneManager.h>
#include <string.h>
#include <string>
#include <unordered_map>

#include "components/LightComponent.h"
#include "components/MeshComponent.h"
#include "components/Transform.h"
#include "context/ResourceContext.h"
#include "core/vulkancore.h"
#include "Entity.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_internal.h"
#include "managers/MaterialManager.h"
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

void InspectorUi::selectEntity(int entityId) {
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
	} else {
		selection.type = InspectorSelectionType::Asset;
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

					Texture* tex = nullptr;
					// Try to get existing texture first using full path key
					try {
						tex = resources.getTextureManager().getTexture(texKey);
					} catch (...) {
						// Load new texture and register it under the full path key
						// Load new texture and register it under the full path key
						tex = resources.getTextureManager().loadTexture(assetPath);
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
	return -1;
}

const std::string& InspectorUi::getSelectedAssetPath() {
	return selection.assetPath;
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

	Material* material = resources.getMaterialManager().getMaterial(normFullPath);
	if (!material) {
		material = resources.getMaterialManager().loadMaterialFromFile(normFullPath);
	}
	if (!material) {
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Failed to load material.");
	} else {
		ImGui::Spacing();

		// Albedo texture section
		const char* albedoLabel = "Albedo";
		ImVec2 albedoSize = ImGui::CalcTextSize(albedoLabel);
		ImVec2 squareSize(albedoSize.y, albedoSize.y);
		ImGui::InvisibleButton("AlbedoTextureDropTarget", squareSize);
		bool isHoveredTex = ImGui::IsItemHovered();
		ImDrawList* dl2 = ImGui::GetWindowDrawList();
		ImVec2 min2 = ImGui::GetItemRectMin();
		ImVec2 max2 = ImGui::GetItemRectMax();
		ImU32 col2 = ImGui::GetColorU32(isHoveredTex ? ImGuiCol_ButtonHovered : ImGuiCol_Border);
		dl2->AddRect(min2, max2, col2, 3.0f);

		// Preview the material's albedo texture (look up by key)
		Texture* previewTex = nullptr;
		try {
			previewTex = resources.getTextureManager().getTexture(material->albedoTextureKey);
		} catch (...) {
			previewTex = nullptr;
		}

		if (previewTex && previewTex->handle.isValid() && previewTex->sampler.isValid()) {
			ImVec2 innerMin2(min2.x + 2.0f, min2.y + 2.0f);
			ImVec2 innerMax2(max2.x - 2.0f, max2.y - 2.0f);
			VkDescriptorSet texSet2 = getOrCreateImGuiTextureSet(previewTex);
			if (texSet2 != VK_NULL_HANDLE) {
				dl2->AddImage(reinterpret_cast<ImTextureID>(texSet2), innerMin2, innerMax2);
			}
		} else {
			// Fallback: inner shadow box for albedo drop target
			float inset2 = 2.0f;
			ImVec2 innerMin2(min2.x + inset2, min2.y + inset2);
			ImVec2 innerMax2(max2.x - inset2, max2.y - inset2);
			ImVec4 shadowBase2 = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
			shadowBase2.w *= 0.9f;
			ImU32 shadowCol2 = ImGui::ColorConvertFloat4ToU32(shadowBase2);
			dl2->AddRectFilled(innerMin2, innerMax2, shadowCol2, 2.0f);
		}

		// Attach drag-drop target to the square
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_FILE")) {
				const char* droppedPath = static_cast<const char*>(payload->Data);
				if (droppedPath && droppedPath[0] != '\0') {
					std::filesystem::path texPath(droppedPath);
					std::string texExt = texPath.extension().string();
					for (auto& c : texExt)
						c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));

					if (texExt == ".png" || texExt == ".jpg" || texExt == ".jpeg" || texExt == ".tga" ||
						texExt == ".bmp" || texExt == ".hdr") {
						// Use full texture path as the key
						std::string texKey = texPath.string();

						Texture* tex = nullptr;
						try {
							tex = resources.getTextureManager().getTexture(texKey);
						} catch (...) {
							tex = resources.getTextureManager().loadTexture(droppedPath);
						}

						// Rebuild or create the material with new albedo texture.
						std::string matName = getMaterialNameFromPath(fullPath);
						// Update material texture (create material if missing)
						std::string normPath;
						normPath = std::filesystem::path(fullPath).generic_string();
						Material* mat = resources.getMaterialManager().updateMaterialTexture(normPath, texKey);
						if (!mat) {
							std::cerr << "Inspector: failed to create/update material for path '" << normPath << "'"
									  << std::endl;
						}

						// Persist updated material back to JSON file
						resources.getMaterialManager().saveMaterialToFile(fullPath,
																		  matName,
																		  texKey,
																		  mat->properties.albedo,
																		  mat->properties.metallic,
																		  mat->properties.roughness,
																		  mat->properties.ao);
						std::cerr << "Inspector: updated material '" << fullPath << "' with texture '" << texKey << "'"
								  << std::endl;
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();
		ImGui::TextUnformatted(albedoLabel);

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
							  &material->properties.albedo.x,
							  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
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
															  material->properties.albedo,
															  material->properties.metallic,
															  material->properties.roughness,
															  material->properties.ao);
		}
	}

	ImGui::Unindent(kContentIndent);
	ImGui::PopStyleVar();
}

void InspectorUi::render() {
	Scene* scene = resources.getSceneManager().getActiveScene();

	ImGui::PushStyleVarX(ImGuiStyleVar_WindowPadding, 0.0f);

	ImGui::Begin("Inspector");

	const float kHeaderContentTopPadding = 6.0f;

	if (selection.type == InspectorSelectionType::Entity && scene && selection.entityId > 0) {
		Entity* entityPtr = nullptr;
		auto* entities = scene->getEntities();
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

		Entity& entity = *entityPtr;
		char nameBuffer[256];
		strncpy_s(nameBuffer, entity.getName().c_str(), sizeof(nameBuffer));
		nameBuffer[sizeof(nameBuffer) - 1] = 0;

		// Name row with padding
		ImGui::Indent(kContentIndent);
		ImGui::Text("Name");
		ImGui::SameLine();
		if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer)))
			entity.setName(nameBuffer);

		ImGui::Unindent(kContentIndent);
		ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 1.0f);
		ImGui::Separator();
		ImGui::PopStyleVar();

		// Transform section
		auto* transform = entity.getComponent<Transform>();
		// Make collapsing header more compact by reducing FramePadding vertically
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.0f);
		if (transform && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::PopStyleVar(3);

			ImGui::Dummy(ImVec2(0.0f, kHeaderContentTopPadding));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kContentSpacing);
			ImGui::Indent(kContentIndent);

			ImGui::Columns(2, nullptr, false);
			ImGui::SetColumnWidth(0, 60.0f);

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

			ImGui::Unindent(kContentIndent);
			ImGui::PopStyleVar();
		} else {
			ImGui::PopStyleVar(3);
		}

		ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 1.0f);
		ImGui::Separator();
		ImGui::PopStyleVar();

		// Mesh / material section
		auto* meshComp = entity.getComponent<MeshComponent>();
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.0f);
		if (meshComp && ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::PopStyleVar(3);

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
									resources.getMaterialManager().saveMaterialToFile(droppedPath, matName, "default");
								}

								meshComp->SetMaterial(mat);
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
		} else {
			ImGui::PopStyleVar(3);
		}

		ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 1.0f);
		ImGui::Separator();
		ImGui::PopStyleVar();

		// Light component section
		auto* lightComp = entity.getComponent<LightComponent>();
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.0f);
		if (lightComp && ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::PopStyleVar(3);

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
			}
			ImGui::NextColumn();

			// Color
			ImGui::Text("Color");
			ImGui::NextColumn();
			ImGui::ColorEdit3(
				"##LightColor", &lightComp->color.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
			ImGui::NextColumn();

			// Intensity
			ImGui::Text("Intensity");
			ImGui::NextColumn();
			ImGui::DragFloat("##LightIntensity", &lightComp->intensity, 0.01f, 0.0f, 100.0f, "%.2f");
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
				ImGui::DragFloat("##LightAttenKc", &lightComp->attenuationKc, 0.01f, 0.0f, 100.0f, "%.3f");
				ImGui::NextColumn();

				// Linear (Kl)
				ImGui::Text("Kl (Linear)");
				ImGui::NextColumn();
				ImGui::DragFloat("##LightAttenKl", &lightComp->attenuationKl, 0.01f, 0.0f, 100.0f, "%.3f");
				ImGui::NextColumn();

				// Quadratic (Kq)
				ImGui::Text("Kq (Quad)");
				ImGui::NextColumn();
				ImGui::DragFloat("##LightAttenKq", &lightComp->attenuationKq, 0.001f, 0.0f, 100.0f, "%.4f");
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
				}
				ImGui::NextColumn();
			}

			ImGui::Unindent(kContentIndent);
			ImGui::PopStyleVar();
		} else {
			ImGui::PopStyleVar(3);
		}

		ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 1.0f);
		ImGui::Separator();
		ImGui::PopStyleVar();
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
	}

	ImGui::PopStyleVar();
	ImGui::End();
}
