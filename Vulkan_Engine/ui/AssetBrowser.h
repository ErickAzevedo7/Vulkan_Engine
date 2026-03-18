#pragma once
#include <imgui_internal.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "managers/TextureManager.h"
#include "vulkan/vulkan_core.h"


struct FileIcon {
	Texture* texture = nullptr;
	VkDescriptorSet imguiTexture = VK_NULL_HANDLE;
};

struct FileEntry {
	std::string name;
	std::string fullPath;
	bool isDirectory;
};

struct FolderNode {
	std::string name;
	std::string fullPath;
	std::vector<FolderNode> children;
	bool scanned = false;
};

class ResourceContext;
class InspectorUi;

class AssetBrowser {
private:
	ResourceContext& resources;
	InspectorUi& inspector;

	FileIcon defaultFileIcon;
	FileIcon textureFileIcon;
	FileIcon meshFileIcon;
	FileIcon folderIcon;
	bool iconsInitialized = false;
	bool firstFrame = true;

	FolderNode rootFolder;
	bool folderTreeInitialized = false;
	std::string selectedFolderPath;
	std::vector<FileEntry> currentFolderEntries;
	char fileFilter[128] = "";
	std::unordered_map<std::string, VkDescriptorSet> thumbnailDescriptorSets;

	std::string resolvedRootPath; // Set in constructor — absolute path to solution-root "projects\"

	void InitFileIcons();
	void ScanCurrentFolderContents();
	void ScanFolder(FolderNode& node);
	void EnsureFolderTreeInitialized();
	void DrawFolderNode(FolderNode& node);
	void DrawSidebar();
	bool PassesFilter(const char* name, const char* filter);
	void DrawFolderContents(const char* fileFilter);
	const ThumbnailTexture* getThumbnailForEntry(const FileEntry& fe);

public:
	AssetBrowser(ResourceContext& resources, InspectorUi& inspector);
	const FileIcon& GetIconForEntry(const FileEntry& fe);
	void render();
};
