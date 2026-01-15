#pragma once
#include "managers/TextureManager.h"
#include "managers/MaterialManager.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include <imgui_internal.h>
#include <filesystem>

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

class AssetBrowser {
 private:
  static FileIcon defaultFileIcon;
  static FileIcon textureFileIcon;
  static FileIcon meshFileIcon;
  static FileIcon folderIcon;
  static bool iconsInitialized;
  static bool firstFrame;

  static FolderNode s_RootFolder;
  static bool s_FolderTreeInitialized;
  static std::string s_SelectedFolderPath;
  static std::vector<FileEntry> s_CurrentFolderEntries;
  static char s_FileFilter[128];

  static constexpr const char* kAssetsRootPath = "assets";

  static void InitFileIcons();
  static void ScanCurrentFolderContents();
  static void ScanFolder(FolderNode& node);
  static void EnsureFolderTreeInitialized();
  static void DrawFolderNode(FolderNode& node);
  static void DrawSidebar();
  static bool PassesFilter(const char* name, const char* filter);
  static void DrawFolderContents(const char* fileFilter);

 public:
  static const FileIcon& GetIconForEntry(const FileEntry& fe);
  static int selectedItemIndex;
  static std::string selectedAssetPath;
	static void render();


};
