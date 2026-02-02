#pragma once

#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>

#include <vulkan/vulkan.h>

#include "Entity.h"
#include "components/LightComponent.h"
#include "components/MeshComponent.h"
#include "components/Transform.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "managers/SceneManager.h"
#include "ui/AssetBrowser.h"

// What the inspector is currently inspecting.
enum class InspectorSelectionType {
	None,
	Entity,
	Asset,
	Material
};

struct InspectorSelection {
	InspectorSelectionType type = InspectorSelectionType::None;
	int entityId = 0; // valid only if type == Entity
	std::string assetPath; // valid only if type == Asset
};

// When selecting an asset we may be "picking" something for the inspector
// rather than just inspecting the asset itself.
enum class InspectorPickTarget {
	None,
	MeshAlbedo
};

class InspectorUi {
public:
	static void render();

	// Selection control API
	static void selectEntity(int entityId);
	static void selectAsset(const std::string& assetPath);
	static void clearSelection();

	// Query helpers
	static int getSelectedEntityId(); // returns -1 if no entity selected
	static const std::string& getSelectedAssetPath();

private:
	static std::vector<VkDescriptorSet> imguiTextureSets;
	static void releaseImGuiTextureSets();

	static InspectorSelection selection;
	static InspectorPickTarget pickTarget;
	static const float kContentIndent;
	static const ImVec2 kContentSpacing;

	// Helper to derive a logical material name from a .mat asset path
	static std::string getMaterialNameFromPath(const std::string& fullPath);

	static VkDescriptorSet getOrCreateImGuiTextureSet(Texture* texture);
	// Draw a collapsing header with an optional icon; returns true when open
	static bool drawIconCollapsingHeader(const char* id,
	                                     ImTextureID iconTex,
	                                     const char* label,
	                                     ImGuiTreeNodeFlags flags = 0);
	static void renderMaterialTab(std::string fullPath);

	static std::unordered_map<const Texture*, VkDescriptorSet> imguiTextureCache;
};
