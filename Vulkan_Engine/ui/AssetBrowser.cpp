#include "AssetBrowser.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <managers/MaterialManager.h>
#include <string>
#include <ui/InspectorUi.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "context/ResourceContext.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_internal.h"
#include "managers/TextureManager.h"
#include "renderer/vulkan/VulkanTexture.h"
#include "vulkan/vulkan_core.h"


AssetBrowser::AssetBrowser(ResourceContext& resources, InspectorUi& inspector)
	: resources(resources), inspector(inspector) {
	// Initialize defaults
	firstFrame = true;
	iconsInitialized = false;
	folderTreeInitialized = false;
	fileFilter[0] = '\0';
}

std::string truncateText(const std::string& p_text, float p_truncated_width) {
	std::string truncated_text = p_text;

	const float text_width = ImGui::CalcTextSize(p_text.c_str(), nullptr, true).x;

	if (text_width > p_truncated_width) {
		constexpr const char* ELLIPSIS = u8"\u2026";
		const float ellipsis_size = ImGui::CalcTextSize(ELLIPSIS).x;

		int visible_chars = 0;
		for (size_t i = 0; i < p_text.size(); i++) {
			const float current_width = ImGui::CalcTextSize(p_text.substr(0, i).c_str(), nullptr, true).x;
			if (current_width + ellipsis_size > p_truncated_width) {
				break;
			}

			visible_chars = static_cast<int>(i);
		}

		truncated_text = (p_text.substr(0, visible_chars) + ELLIPSIS).c_str();
	}

	return truncated_text;
}

void AssetBrowser::ScanCurrentFolderContents() {
	namespace fs = std::filesystem;
	currentFolderEntries.clear();

	fs::path path(selectedFolderPath);
	if (!fs::exists(path) || !fs::is_directory(path)) {
		return;
	}

	for (const auto& entry : fs::directory_iterator(path)) {
		FileEntry fe;
		fe.name = entry.path().filename().string();
		fe.fullPath = entry.path().string();
		fe.isDirectory = entry.is_directory();
		currentFolderEntries.push_back(std::move(fe));
	}

	std::sort(currentFolderEntries.begin(), currentFolderEntries.end(), [](const FileEntry& a, const FileEntry& b) {
		// Directories first, then alphabetical
		if (a.isDirectory != b.isDirectory) {
			return a.isDirectory && !b.isDirectory;
		}
		return a.name < b.name;
	});
}

void AssetBrowser::ScanFolder(FolderNode& node) {
	namespace fs = std::filesystem;

	node.children.clear();
	node.scanned = true;

	fs::path path(node.fullPath);
	if (!fs::exists(path) || !fs::is_directory(path)) {
		return;
	}

	for (const auto& entry : fs::directory_iterator(path)) {
		if (!entry.is_directory()) {
			continue;
		}
		FolderNode child;
		child.name = entry.path().filename().string();
		child.fullPath = entry.path().string();
		child.scanned = false;
		node.children.push_back(std::move(child));
	}

	std::sort(node.children.begin(), node.children.end(), [](const FolderNode& a, const FolderNode& b) {
		return a.name < b.name;
	});
}

void AssetBrowser::EnsureFolderTreeInitialized() {
	if (folderTreeInitialized) {
		return;
	}

	rootFolder.name = "assets";
	rootFolder.fullPath = kAssetsRootPath;
	rootFolder.scanned = false;
	selectedFolderPath = rootFolder.fullPath;

	ScanFolder(rootFolder);
	ScanCurrentFolderContents();
	folderTreeInitialized = true;
}

void AssetBrowser::DrawFolderNode(FolderNode& node) {
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;

	if (selectedFolderPath == node.fullPath) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	bool open = ImGui::TreeNodeEx(node.name.c_str(), flags);

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		selectedFolderPath = node.fullPath;
		ScanCurrentFolderContents();
	}

	if (open) {
		if (!node.scanned) {
			ScanFolder(node);
		}
		for (auto& child : node.children) {
			DrawFolderNode(child);
		}
		ImGui::TreePop();
	}
}

void AssetBrowser::DrawSidebar() {
	EnsureFolderTreeInitialized();

	ImGui::TextUnformatted("Folders");

	DrawFolderNode(rootFolder);
}

bool AssetBrowser::PassesFilter(const char* name, const char* filter) {
	if (filter[0] == '\0') {
		return true;
	}
	return ImStristr(name, nullptr, filter, nullptr) != nullptr;
}

void AssetBrowser::DrawFolderContents(const char* fileFilter) {
	ImGui::TextUnformatted(selectedFolderPath.c_str());
	ImGui::Separator();

	const float iconSize = 48.0f;
	const float cellPadding = 12.0f;
	const float labelHeight = 16.0f;

	const float itemWidth = iconSize + cellPadding * 2.0f;
	const float itemHeight = iconSize + labelHeight + cellPadding;

	float totalWidth = ImGui::GetContentRegionAvail().x;
	int columns = static_cast<int>(totalWidth / (itemWidth + cellPadding));
	columns = std::max(columns, 1);

	int index = 0;
	ImGui::BeginChild("FolderContentGrid", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing() * 2), false);

	// Context menu (right-click) for current folder over the whole content area
	if (ImGui::BeginPopupContextWindow("AssetBrowserFolderContext",
									   ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
		if (ImGui::MenuItem("Create Material")) {
			std::string baseName = "NewMaterial";
			std::string newPath;
			int index = 0;
			while (true) {
				std::string fileName = baseName;
				if (index > 0) {
					fileName += "_" + std::to_string(index);
				}
				fileName += ".mat";
				newPath = selectedFolderPath + "/" + fileName;
				if (!std::filesystem::exists(newPath)) {
					break;
				}
				++index;
			}

			// Determine material logical name from filename (ensure unique name)
			std::filesystem::path newPathFs(newPath);
			std::string materialName = newPathFs.stem().string();

			// Save file and create material in memory immediately with default texture
			resources.getMaterialManager().saveMaterialToFile(
				newPath, materialName, resources.getTextureManager().kDefaultTextureKey);
			resources.getMaterialManager().createMaterial(
				materialName, resources.getTextureManager().kDefaultTextureKey, newPath);
			ScanCurrentFolderContents();
		}
		ImGui::EndPopup();
	}

	int itemIndex = 0;
	for (const FileEntry& fe : currentFolderEntries) {
		if (!PassesFilter(fe.name.c_str(), fileFilter)) {
			continue;
		}

		if (index % columns != 0) {
			ImGui::SameLine();
		}

		ImGui::BeginGroup();
		ImGui::PushID(itemIndex++);

		ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImVec2 size(itemWidth, itemHeight);

		const std::string& selectedAssetPath = inspector.getSelectedAssetPath();
		bool selected = (selectedAssetPath == fe.fullPath);

		if (selected) {
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec4 selCol = ImGui::GetStyleColorVec4(ImGuiCol_Header);
			ImU32 col = ImGui::ColorConvertFloat4ToU32(selCol);

			ImVec2 p0 = cursor;
			ImVec2 p1 = ImVec2(cursor.x + size.x, cursor.y + size.y);
			drawList->AddRectFilled(p0, p1, col, 4.0f); // 4.0f = corne

			ImVec4 selectionColor = ImGui::GetStyleColorVec4(ImGuiCol_Header);
		}

		// Draw icon
		ImVec2 iconPos(cursor.x + (size.x - iconSize) * 0.5f, cursor.y + cellPadding * 0.5f);
		ImGui::SetCursorScreenPos(iconPos);

		{
			// Prefer real thumbnail texture if this is an image file already loaded
			const ThumbnailTexture* thumbTex = getThumbnailForEntry(fe);
			if (thumbTex && thumbTex->handle.isValid() && thumbTex->sampler.isValid()) {
				VkDescriptorSet thumbSet = VK_NULL_HANDLE;
				std::string thumbKey;
				thumbKey = std::filesystem::path(fe.fullPath).generic_string();
				auto it = thumbnailDescriptorSets.find(thumbKey);
				if (it != thumbnailDescriptorSets.end()) {
					thumbSet = it->second;
				} else {
					auto* tm = &resources.getTextureManager();
					if (auto* vt = static_cast<Renderer::VulkanTexture*>(tm->getGraphicsTexture())) {
						VkSampler sampler = vt->getSampler(thumbTex->sampler);
						VkImageView imageView = vt->getImageView(thumbTex->handle);
						thumbSet =
							ImGui_ImplVulkan_AddTexture(sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
						thumbnailDescriptorSets[thumbKey] = thumbSet;
					}
				}
				ImGui::Image(reinterpret_cast<ImTextureID>(thumbSet), ImVec2(iconSize, iconSize));
			} else {
				const FileIcon& icon = GetIconForEntry(fe);
				if (icon.imguiTexture != VK_NULL_HANDLE) {
					ImGui::Image(reinterpret_cast<ImTextureID>(icon.imguiTexture), ImVec2(iconSize, iconSize));
				} else {
					ImGui::TextUnformatted(fe.isDirectory ? "[DIR]" : "[FILE]");
				}
			}
		}

		// Draw label text only (no selectable here)
		ImGui::SetCursorScreenPos(ImVec2(cursor.x + cellPadding * 0.5f, cursor.y + size.y - labelHeight));

		const float textWidth = size.x - 8.0f;
		const std::string truncated = truncateText(fe.name.c_str(), textWidth);
		ImGui::TextUnformatted(truncated.c_str());

		ImGui::SetCursorScreenPos(cursor);

		// Temporarily disable hover/active background
		// Transparent hover when NOT selected
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));

		// Keep active equal to selection (or tweak if you want a press effect)
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));

		// Selection color (visible background for selected items)
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));

		const ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowDoubleClick;

		if (ImGui::Selectable("##AssetTile", selected, flags, size)) {
			// Let inspector know this asset is now the active selection
			inspector.selectAsset(fe.fullPath);

			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				if (fe.isDirectory) {
					selectedFolderPath = fe.fullPath;
					ScanCurrentFolderContents();
				} else {
					// Open material assets when double-clicking .mat files
					std::string ext;
					const size_t dotPos = fe.name.find_last_of('.');
					if (dotPos != std::string::npos && dotPos + 1 < fe.name.size()) {
						ext = fe.name.substr(dotPos + 1);
						for (char& c : ext) {
							c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
						}
					}

					if (ext == "mat") {
						Material* material = resources.getMaterialManager().loadMaterialFromFile(fe.fullPath);
						if (material) {
							// Here you can add a dedicated Inspector selection for materials
							// For now we just select the asset path so user sees it in Inspector
							inspector.selectAsset(fe.fullPath);
						}
					}
				}
			}
		}

		// Begin drag from the selected tile (files only) so clicks still work
		if (!fe.isDirectory && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			const std::string path = fe.fullPath;
			ImGui::SetDragDropPayload("ASSET_BROWSER_FILE", path.c_str(), path.size() + 1);
			ImGui::TextUnformatted(fe.name.c_str());
			ImGui::EndDragDropSource();
		}

		// Restore previous colors
		ImGui::PopStyleColor(3);

		ImGui::PopID();
		ImGui::EndGroup();

		index++;
	}

	ImGui::EndChild();
}

void AssetBrowser::InitFileIcons() {
	if (iconsInitialized) {
		return;
	}

	Texture* defaultFile = resources.getTextureManager().getTexture("defaultFile");
	Texture* folder = resources.getTextureManager().getTexture("folder");

	defaultFileIcon.texture = defaultFile;
	textureFileIcon.texture = defaultFile;
	meshFileIcon.texture = defaultFile;
	folderIcon.texture = folder;

	// Criar VkDescriptorSet para ImGui a partir dos VkImageView/VkSampler
	auto makeImguiTexture = [&](Texture* tex) -> VkDescriptorSet {
		if (!tex) {
			return VK_NULL_HANDLE;
		}

		auto* tm = &resources.getTextureManager();
		if (auto* vt = static_cast<Renderer::VulkanTexture*>(tm->getGraphicsTexture())) {
			VkSampler sampler = vt->getSampler(tex->sampler);
			VkImageView imageView = vt->getImageView(tex->handle);
			return ImGui_ImplVulkan_AddTexture(sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		return VK_NULL_HANDLE;
	};

	defaultFileIcon.imguiTexture = makeImguiTexture(defaultFileIcon.texture);
	textureFileIcon.imguiTexture = makeImguiTexture(textureFileIcon.texture);
	meshFileIcon.imguiTexture = makeImguiTexture(meshFileIcon.texture);
	folderIcon.imguiTexture = makeImguiTexture(folderIcon.texture);

	iconsInitialized = true;
}

const FileIcon& AssetBrowser::GetIconForEntry(const FileEntry& fe) {
	if (fe.isDirectory) {
		return folderIcon;
	}

	// Descobrir extens�o
	std::string ext;
	const size_t dotPos = fe.name.find_last_of('.');
	if (dotPos != std::string::npos && dotPos + 1 < fe.name.size()) {
		ext = fe.name.substr(dotPos + 1);
		for (char& c : ext) {
			c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
		}
	}

	// Texturas
	if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tga" || ext == "bmp" || ext == "hdr") {
		return textureFileIcon;
	}

	// Meshes
	if (ext == "obj" || ext == "fbx" || ext == "gltf" || ext == "glb") {
		return meshFileIcon;
	}

	// Fallback
	return defaultFileIcon;
}

const ThumbnailTexture* AssetBrowser::getThumbnailForEntry(const FileEntry& fe) {
	if (fe.isDirectory) {
		return nullptr;
	}

	// Discover extension
	std::string ext;
	const size_t dotPos = fe.name.find_last_of('.');
	if (dotPos != std::string::npos && dotPos + 1 < fe.name.size()) {
		ext = fe.name.substr(dotPos + 1);
		for (char& c : ext) {
			c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
		}
	}

	if (ext != "png" && ext != "jpg" && ext != "jpeg" && ext != "tga" && ext != "bmp" && ext != "hdr") {
		return nullptr;
	}

	// Thumbnails were precomputed in TextureManager with fullPath as key
	return resources.getTextureManager().getThumbnail(fe.fullPath);
}

void AssetBrowser::render() {
	InitFileIcons();

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;

	if (!ImGui::Begin("Assets", nullptr, windowFlags)) {
		ImGui::End();
		return;
	}

	ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, 0.0f);

	// Top search bar (before columns)
	ImGui::BeginMenuBar();
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	ImGui::InputText("##FileSearch", fileFilter, IM_ARRAYSIZE(fileFilter));
	ImGui::Separator();
	ImGui::EndMenuBar();

	// Two-panel layout: sidebar + main content
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));

	ImGui::Columns(2, nullptr, true);

	if (firstFrame) {
		ImGui::SetColumnWidth(0, 220.0f);
		firstFrame = false;
	}

	DrawSidebar();

	ImGui::NextColumn();
	ImGui::BeginChild("AssetMainPanel", ImVec2(0.0f, 0.0f), false);

	DrawFolderContents(fileFilter);

	ImGui::EndChild();

	ImGui::Columns(1);
	ImGui::PopStyleVar(2);

	ImGui::PopStyleVar();
	ImGui::End();
}
