#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "imgui.h"
#include "managers/TextureManager.h"
#include "vulkan/vulkan_core.h"

class AssetBrowser;

// What the inspector is currently inspecting.
enum class InspectorSelectionType { None, Entity, Asset, Material };

struct InspectorSelection {
	InspectorSelectionType type = InspectorSelectionType::None;
	int entityId = 0; // valid only if type == Entity
	std::string assetPath; // valid only if type == Asset
};

// When selecting an asset we may be "picking" something for the inspector
// rather than just inspecting the asset itself.
enum class InspectorPickTarget { None, MeshAlbedo };

class ResourceContext; // Forward declaration

class InspectorUi {
public:
	InspectorUi(ResourceContext& resources);
	~InspectorUi();

	void render();

	// Selection control API
	void selectEntity(int entityId);
	void selectAsset(const std::string& assetPath);
	void clearSelection();

	// Query helpers
	int getSelectedEntityId(); // returns -1 if no entity selected
	const std::string& getSelectedAssetPath();

	void setAssetBrowser(AssetBrowser* browser) {
		assetBrowser = browser;
	}

private:
	ResourceContext& resources;
	AssetBrowser* assetBrowser = nullptr;
	std::vector<VkDescriptorSet> imguiTextureSets;
	void releaseImGuiTextureSets();

	InspectorSelection selection;
	InspectorPickTarget pickTarget;
	const float kContentIndent;
	const ImVec2 kContentSpacing;

	// Helper to derive a logical material name from a .mat asset path
	std::string getMaterialNameFromPath(const std::string& fullPath);

	VkDescriptorSet getOrCreateImGuiTextureSet(Texture* texture);
	// Draw a collapsing header with an optional icon; returns true when open
	bool drawIconCollapsingHeader(const char* id, ImTextureID iconTex, const char* label, ImGuiTreeNodeFlags flags = 0);
	void renderMaterialTab(std::string fullPath);

	std::unordered_map<const Texture*, VkDescriptorSet> imguiTextureCache;
};
