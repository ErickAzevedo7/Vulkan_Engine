#include "AssetBrowserPanel.h"

#include <algorithm>
#include <assets/AssetManager.h>
#include <cctype>
#include <cmath>
#include <core/Log.h>
#include <cstring>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <renderer/Overlay.h>
#include <scene/GameObject.h>
#include <scene/Scene.h>
#include <ui/Icons.h>
#include <ui/Style.h>
#include <ui/UISystem.h>

#include "EditorScene.h"
#include "Fields.h"
#include "MaterialEdits.h"
#include "Widgets.h"

namespace VKE::Editor {

    using UI::GetFont;
    using UI::TextStyle;
    using UI::ToImColor;
    namespace Colors = UI::Colors;
    namespace Icons = UI::Icons;
    namespace Sizes = UI::Sizes;

    namespace {
        // Drag-and-drop payload of a folder, carrying its path in UTF-8.
        constexpr const char* kFolderPayload = "VKE_FOLDER";
        constexpr f32 kTileWidth = 84.0f;
        constexpr f32 kThumbSize = 64.0f;
        // Two lines of the label and one of the kind under the thumbnail.
        constexpr f32 kTileHeight =
            Sizes::Space2 + kThumbSize + Sizes::Space2 + 28.0f + Sizes::Space1 + 12.0f + Sizes::Space3;
        constexpr f32 kIconWidth = 14.0f;
        constexpr f32 kSplitterWidth = 4.0f;

        std::string WithIcon(const char* icon, const char* label) {
            return std::string(icon) + "  " + label;
        }

        bool ContainsIgnoringCase(const std::string& text, const std::string& part) {
            auto it = std::search(text.begin(), text.end(), part.begin(), part.end(), [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });
            return it != text.end();
        }

        bool LessIgnoringCase(const std::string& a, const std::string& b) {
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) {
                return std::tolower(static_cast<unsigned char>(x)) < std::tolower(static_cast<unsigned char>(y));
            });
        }

        const char* GetTypeIcon(Assets::AssetType type) {
            switch (type) {
            case Assets::AssetType::Model: return Icons::Boxes;
            case Assets::AssetType::Texture: return Icons::Image;
            case Assets::AssetType::Material: return Icons::Palette;
            case Assets::AssetType::Scene: return Icons::Clapperboard;
            case Assets::AssetType::Mesh: return Icons::Box;
            case Assets::AssetType::Prefab: return Icons::Package;
            }
            return Icons::File;
        }

        const char* GetTypeName(Assets::AssetType type) {
            switch (type) {
            case Assets::AssetType::Model: return "MODEL";
            case Assets::AssetType::Texture: return "TEXTURE";
            case Assets::AssetType::Material: return "MATERIAL";
            case Assets::AssetType::Scene: return "SCENE";
            case Assets::AssetType::Mesh: return "MESH";
            case Assets::AssetType::Prefab: return "PREFAB";
            }
            return "";
        }

        bool IsFolder(const std::filesystem::path& path) {
            std::error_code ec;
            return std::filesystem::is_directory(path, ec);
        }

        // The folders in `folder`, sorted by name.
        std::vector<std::filesystem::path> ListFolders(const std::filesystem::path& folder) {
            std::vector<std::filesystem::path> folders;
            std::error_code ec;
            for (auto it = std::filesystem::directory_iterator(folder, ec);
                 !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
                if (it->is_directory(ec)) folders.push_back(it->path());
            }
            std::sort(folders.begin(), folders.end(), [](const auto& a, const auto& b) {
                return LessIgnoringCase(a.filename().u8string(), b.filename().u8string());
            });
            return folders;
        }

        // A name not taken in `folder`: "New Folder", "New Folder 2", ...
        std::filesystem::path MakeUniquePath(const std::filesystem::path& folder, const std::string& stem,
                                             const std::string& extension) {
            std::error_code ec;
            std::filesystem::path path = folder / std::filesystem::u8path(stem + extension);
            for (u32 suffix = 2; std::filesystem::exists(path, ec); ++suffix) {
                path = folder / std::filesystem::u8path(stem + " " + std::to_string(suffix) + extension);
            }
            return path;
        }

        // Splits `text` into two lines that fit `width`, the second cut with an ellipsis.
        void DrawTwoLines(ImFont* font, const std::string& text, f32 centerX, f32 top, f32 width, u32 color) {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            auto measure = [&](const char* begin, const char* end) {
                return font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, begin, end).x;
            };
            const char* begin = text.c_str();
            const char* end = begin + text.size();
            const char* cut = end;
            while (cut > begin && measure(begin, cut) > width) {
                --cut;
            }
            // Breaks after a space or underscore near the cut, if there is one.
            if (cut < end) {
                for (const char* c = cut; c > begin + (cut - begin) / 2; --c) {
                    if (c[-1] == ' ' || c[-1] == '_' || c[-1] == '-') {
                        cut = c;
                        break;
                    }
                }
            }
            f32 firstWidth = measure(begin, cut);
            draw->AddText(font, font->FontSize, ImVec2(std::floor(centerX - firstWidth * 0.5f), top), ToImColor(color),
                          begin, cut);
            if (cut == end) return;

            std::string second(cut, end);
            if (measure(second.c_str(), nullptr) > width) {
                const char* ellipsis = "\xe2\x80\xa6";
                f32 ellipsisWidth = measure(ellipsis, nullptr);
                while (!second.empty() && measure(second.c_str(), nullptr) + ellipsisWidth > width) {
                    second.pop_back();
                    // Not in the middle of a UTF-8 sequence.
                    while (!second.empty() && (static_cast<unsigned char>(second.back()) & 0xC0) == 0x80) {
                        second.pop_back();
                    }
                }
                second += ellipsis;
            }
            f32 secondWidth = measure(second.c_str(), nullptr);
            draw->AddText(font, font->FontSize,
                          ImVec2(std::floor(centerX - secondWidth * 0.5f), top + font->FontSize + 0.0f),
                          ToImColor(color), second.c_str());
        }
    } // namespace

    AssetBrowserPanel::AssetBrowserPanel(EditorScene& scene, MaterialEdits& edits, UI::UISystem& ui)
        : m_Scene(scene), m_Edits(edits), m_UI(ui) {
    }

    const char* AssetBrowserPanel::GetWindowName() {
        static const std::string name = std::string(Icons::Boxes) + "  Assets###Assets";
        return name.c_str();
    }

    Assets::AssetID AssetBrowserPanel::TakeSceneToOpen() {
        Assets::AssetID scene = m_SceneToOpen;
        m_SceneToOpen = {};
        return scene;
    }

    bool AssetBrowserPanel::IsReadOnly(const std::filesystem::path& path) const {
        const Assets::AssetRoot* root = m_Scene.GetAssets().FindRoot(path);
        // A root folder itself isn't under its root.
        for (const Assets::AssetRoot& candidate : m_Scene.GetAssets().GetRoots()) {
            if (candidate.Folder == path) return candidate.ReadOnly;
        }
        return !root || root->ReadOnly;
    }

    bool AssetBrowserPanel::IsSelected(const Item& item) const {
        // A folder only while nothing else is selected.
        if (item.IsFolder) {
            return item.Path == m_SelectedPath && !m_Scene.GetSelectedAsset().IsValid() && !m_Scene.GetSelected();
        }
        return item.ID == m_Scene.GetSelectedAsset();
    }

    void AssetBrowserPanel::Draw(bool* open) {
        m_LoadedThumbnail = false;
        const std::vector<Assets::AssetRoot>& roots = m_Scene.GetAssets().GetRoots();
        if (m_Folder.empty() && !roots.empty()) m_Folder = roots.front().Folder;
        // The open folder was moved or deleted.
        if (!m_Folder.empty() && !IsFolder(m_Folder) && !roots.empty()) m_Folder = roots.front().Folder;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        bool visible = ImGui::Begin(GetWindowName(), open);
        ImGui::PopStyleVar();
        if (!visible) {
            ImGui::End();
            return;
        }
        m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        UpdateListing();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        f32 height = ImGui::GetContentRegionAvail().y;
        m_TreeWidth = std::clamp(m_TreeWidth, 120.0f, std::max(120.0f, ImGui::GetContentRegionAvail().x - 200.0f));
        ImGui::BeginChild("##Folders", ImVec2(m_TreeWidth, height));
        DrawFolderTree();
        ImGui::EndChild();

        // The splitter between the tree and the content.
        ImGui::SameLine();
        ImVec2 splitterMin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##Splitter", ImVec2(kSplitterWidth, height));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive()) m_TreeWidth += ImGui::GetIO().MouseDelta.x;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(splitterMin.x + kSplitterWidth * 0.5f, splitterMin.y),
                                            ImVec2(splitterMin.x + kSplitterWidth * 0.5f, splitterMin.y + height),
                                            ToImColor(ImGui::IsItemActive() ? Colors::Accent : Colors::Line));

        ImGui::SameLine();
        ImGui::BeginGroup();
        DrawToolbar();
        DrawContent();
        ImGui::EndGroup();
        ImGui::PopStyleVar();

        HandleShortcuts();
        DrawDeleteDialog();
        ImGui::End();
    }

    void AssetBrowserPanel::UpdateListing() {
        if (m_ListedFolder == m_Folder && m_ListedFilter == m_Filter && m_ListedGeneration == m_Generation) return;
        m_ListedFolder = m_Folder;
        m_ListedFilter = m_Filter;
        m_ListedGeneration = m_Generation;
        m_Items.clear();

        Assets::AssetManager& assets = m_Scene.GetAssets();
        auto add = [&](const std::filesystem::path& path, bool isFolder) {
            Item item;
            item.Path = path;
            item.IsFolder = isFolder;
            item.Name = isFolder ? path.filename().u8string() : path.stem().u8string();
            if (!isFolder) {
                item.ID = assets.GetAsset(path);
                std::optional<Assets::AssetType> type = assets.GetType(item.ID);
                if (!type) return;
                item.Type = *type;
            }
            if (!m_Filter.empty() && !ContainsIgnoringCase(item.Name, m_Filter)) return;
            m_Items.push_back(std::move(item));
        };

        // A filter searches the folders under the open one too.
        std::error_code ec;
        auto options = std::filesystem::directory_options::skip_permission_denied;
        if (m_Filter.empty()) {
            for (auto it = std::filesystem::directory_iterator(m_Folder, options, ec);
                 !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
                add(it->path(), it->is_directory(ec));
            }
        } else {
            for (auto it = std::filesystem::recursive_directory_iterator(m_Folder, options, ec);
                 !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
                add(it->path(), it->is_directory(ec));
            }
        }

        // Folders first, then by name.
        std::sort(m_Items.begin(), m_Items.end(), [](const Item& a, const Item& b) {
            if (a.IsFolder != b.IsFolder) return a.IsFolder;
            return LessIgnoringCase(a.Name, b.Name);
        });

        // The meshes and materials of open models, after their tiles.
        for (size_t i = 0; i < m_Items.size(); ++i) {
            if (m_Items[i].IsFolder || m_Items[i].Type != Assets::AssetType::Model) continue;
            if (!m_OpenModels.count(m_Items[i].ID)) continue;
            std::vector<Item> inside;
            for (const Assets::SubAsset& subAsset : assets.GetSubAssets(m_Items[i].ID)) {
                Item item;
                item.Path = m_Items[i].Path;
                item.Name = subAsset.Name;
                item.ID = subAsset.ID;
                item.Type = subAsset.Type;
                item.IsSubAsset = true;
                inside.push_back(std::move(item));
            }
            m_Items.insert(m_Items.begin() + static_cast<std::ptrdiff_t>(i) + 1, inside.begin(), inside.end());
            i += inside.size();
        }
    }

    void AssetBrowserPanel::DrawFolderTree() {
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space2));
        for (const Assets::AssetRoot& root : m_Scene.GetAssets().GetRoots()) {
            DrawFolderRow(root.Folder, root.Name, 0, true, root.ReadOnly);
        }
    }

    void AssetBrowserPanel::DrawFolderRow(const std::filesystem::path& folder, const std::string& name, u32 depth,
                                          bool isRoot, bool readOnly) {
        std::string key = folder.generic_u8string();
        std::vector<std::filesystem::path> children = ListFolders(folder);
        bool open = !children.empty() && m_OpenFolders.count(key) != 0;
        bool current = folder == m_Folder;

        ImGui::PushID(key.c_str());
        ImVec2 min = ImGui::GetCursorScreenPos();
        f32 width = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
        ImVec2 max(min.x + width, min.y + Sizes::RowHeight);
        f32 x = min.x + Sizes::Space4 + static_cast<f32>(depth) * Sizes::Indent;

        ImGui::SetNextItemAllowOverlap();
        if (ImGui::InvisibleButton("##Row", ImVec2(width, Sizes::RowHeight))) {
            m_Folder = folder;
            m_Filter.clear();
        }
        bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem);
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !children.empty()) {
            if (open) {
                m_OpenFolders.erase(key);
            } else {
                m_OpenFolders.insert(key);
            }
        }
        bool dropping = !readOnly && AcceptMove(folder);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (dropping) {
            draw->AddRectFilled(min, max, ToImColor(Colors::AccentSoft));
            draw->AddRect(min, max, ToImColor(Colors::Accent));
        } else if (current) {
            draw->AddRectFilled(min, max, ToImColor(m_Focused ? Colors::AccentSoft : Colors::SelectionUnfocused));
        } else if (hovered) {
            draw->AddRectFilled(min, max, ToImColor(Colors::BgHover));
        }

        ImFont* font = GetFont(TextStyle::UI);
        f32 textY = min.y + std::floor((Sizes::RowHeight - font->FontSize) * 0.5f);
        if (!children.empty()) {
            ImGui::SetCursorScreenPos(ImVec2(x, min.y));
            if (ImGui::InvisibleButton("##Chevron", ImVec2(kIconWidth, Sizes::RowHeight))) {
                if (open) {
                    m_OpenFolders.erase(key);
                } else {
                    m_OpenFolders.insert(key);
                }
                open = !open;
            }
            draw->AddText(font, font->FontSize, ImVec2(x, textY),
                          ToImColor(ImGui::IsItemHovered() ? Colors::Text : Colors::TextMuted),
                          open ? Icons::ChevronDown : Icons::ChevronRight);
        }
        x += kIconWidth + Sizes::Space2;
        const char* icon = isRoot && readOnly ? Icons::Lock : open || current ? Icons::FolderOpen : Icons::Folder;
        draw->AddText(font, font->FontSize, ImVec2(x, textY), ToImColor(Colors::TextMuted), icon);
        x += kIconWidth + Sizes::Space3;

        // A read-only root's badge on the right, the name cut before it.
        f32 clipX = max.x - Sizes::Space4;
        if (isRoot && readOnly) {
            ImFont* small = GetFont(TextStyle::UISmall);
            const char* badge = "READ-ONLY";
            ImVec2 badgeSize = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, badge);
            ImVec2 badgeMin(max.x - Sizes::Space4 - badgeSize.x - 10.0f,
                            min.y + std::floor((Sizes::RowHeight - 16.0f) * 0.5f));
            ImVec2 badgeMax(badgeMin.x + badgeSize.x + 10.0f, badgeMin.y + 16.0f);
            draw->AddRect(badgeMin, badgeMax, ToImColor(Colors::Line), 2.0f);
            draw->AddText(small, small->FontSize,
                          ImVec2(badgeMin.x + 5.0f, badgeMin.y + std::floor((16.0f - badgeSize.y) * 0.5f)),
                          ToImColor(Colors::TextMuted), badge);
            clipX = badgeMin.x - Sizes::Space3;
        }
        ImVec4 clip(x, min.y, clipX, max.y);
        draw->AddText(font, font->FontSize, ImVec2(x, textY), ToImColor(current ? Colors::Text : Colors::TextMuted),
                      name.c_str(), nullptr, 0.0f, &clip);

        ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        if (open) {
            for (const std::filesystem::path& child : children) {
                DrawFolderRow(child, child.filename().u8string(), depth + 1, false, readOnly);
            }
        }
        ImGui::PopID();
    }

    void AssetBrowserPanel::DrawToolbar() {
        ImVec2 min = ImGui::GetCursorScreenPos();
        f32 width = ImGui::GetContentRegionAvail().x;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImGui::Dummy(ImVec2(width, Sizes::ToolbarHeight));
        draw->AddLine(ImVec2(min.x, min.y + Sizes::ToolbarHeight - 1.0f),
                      ImVec2(min.x + width, min.y + Sizes::ToolbarHeight - 1.0f), ToImColor(Colors::Line));
        f32 controlY = min.y + std::floor((Sizes::ToolbarHeight - Sizes::ControlHeight) * 0.5f);

        // The view toggle and the search on the right.
        f32 right = min.x + width - Sizes::Space4;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Sizes::Space1, 0.0f));
        right -= 2.0f * Sizes::ControlHeight + Sizes::Space1;
        ImGui::SetCursorScreenPos(ImVec2(right, controlY));
        if (Widgets::ToggleButton(Icons::LayoutGrid, !m_ListView)) m_ListView = false;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tiles");
        ImGui::SameLine();
        if (Widgets::ToggleButton(Icons::List, m_ListView)) m_ListView = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("List");
        ImGui::PopStyleVar();

        f32 searchWidth = std::min(220.0f, std::max(80.0f, width * 0.35f));
        right -= Sizes::Space4 + searchWidth;
        ImGui::SetCursorScreenPos(ImVec2(right, controlY));
        Widgets::SearchField("##Search", m_Filter, "Search assets", searchWidth);

        // The open folder's path from its root, each part a link to its folder.
        const Assets::AssetRoot* root = nullptr;
        for (const Assets::AssetRoot& candidate : m_Scene.GetAssets().GetRoots()) {
            if (candidate.Folder == m_Folder || m_Scene.GetAssets().FindRoot(m_Folder) == &candidate) root = &candidate;
        }
        if (!root) return;
        std::vector<std::pair<std::string, std::filesystem::path>> parts = {{root->Name, root->Folder}};
        std::filesystem::path relative = m_Folder.lexically_relative(root->Folder);
        std::filesystem::path walked = root->Folder;
        for (const std::filesystem::path& part : relative) {
            if (part == ".") continue;
            walked /= part;
            parts.push_back({part.u8string(), walked});
        }

        ImFont* font = GetFont(TextStyle::UI);
        f32 x = min.x + Sizes::Space4;
        f32 textY = min.y + std::floor((Sizes::ToolbarHeight - font->FontSize) * 0.5f);
        f32 limit = right - Sizes::Space4;
        for (size_t i = 0; i < parts.size() && x < limit; ++i) {
            bool last = i + 1 == parts.size();
            const std::string& name = parts[i].first;
            f32 nameWidth = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, name.c_str()).x;
            ImGui::SetCursorScreenPos(ImVec2(x, textY));
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::InvisibleButton("##Crumb", ImVec2(std::max(nameWidth, 1.0f), font->FontSize)) && !last) {
                m_Folder = parts[i].second;
                m_Filter.clear();
            }
            bool hovered = ImGui::IsItemHovered();
            bool dropping = !last && !IsReadOnly(parts[i].second) && AcceptMove(parts[i].second);
            ImGui::PopID();
            ImVec4 clip(x, min.y, limit, min.y + Sizes::ToolbarHeight);
            u32 color = last || hovered || dropping ? Colors::Text : Colors::TextMuted;
            draw->AddText(GetFont(last ? TextStyle::UIMedium : TextStyle::UI), font->FontSize, ImVec2(x, textY),
                          ToImColor(dropping ? Colors::Accent : color), name.c_str(), nullptr, 0.0f, &clip);
            x += nameWidth + Sizes::Space2;
            if (!last) {
                draw->AddText(font, font->FontSize, ImVec2(x, textY), ToImColor(Colors::TextFaint), Icons::ChevronRight,
                              nullptr, 0.0f, &clip);
                x += kIconWidth + Sizes::Space2;
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(min.x, min.y + Sizes::ToolbarHeight));
    }

    void AssetBrowserPanel::DrawContent() {
        ImGui::BeginChild("##Content");
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Sizes::Space2, Sizes::Space2));
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space2));
        if (m_Items.empty()) {
            ImGui::SetCursorPosX(Sizes::Space5);
            Widgets::Text(m_Filter.empty() ? "This folder is empty." : "No assets match the search.",
                          Colors::TextFaint);
        }

        if (m_ListView) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            for (const Item& item : m_Items) {
                DrawListRow(item);
            }
            ImGui::PopStyleVar();
        } else {
            f32 available = ImGui::GetContentRegionAvail().x - 2.0f * Sizes::Space4;
            u32 columns = std::max(1u, static_cast<u32>((available + Sizes::Space2) / (kTileWidth + Sizes::Space2)));
            for (size_t i = 0; i < m_Items.size(); ++i) {
                if (i % columns == 0) {
                    ImGui::SetCursorPosX(Sizes::Space4);
                } else {
                    ImGui::SameLine();
                }
                DrawTile(m_Items[i], kTileWidth);
            }
        }

        // The space under the items: a click deselects, the menu creates.
        ImVec2 rest = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPosX(0.0f);
        ImGui::InvisibleButton("##Empty", ImVec2(std::max(rest.x, 1.0f), std::max(rest.y, Sizes::RowHeight)));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            m_SelectedPath.clear();
            if (m_Scene.GetSelectedAsset().IsValid()) m_Scene.SelectAsset({});
        }
        if (ImGui::BeginPopupContextItem("##EmptyMenu")) {
            DrawEmptyMenu();
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();

        // Anything dropped in the content lands in the open folder.
        if (!IsReadOnly(m_Folder) && AcceptMove(m_Folder)) {
            ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                                ToImColor(Colors::Accent));
        }
    }

    void AssetBrowserPanel::DrawTile(const Item& item, f32 width) {
        ImGui::PushID(item.IsSubAsset ? item.ID.ToString().c_str() : item.Path.generic_u8string().c_str());
        ImVec2 min = ImGui::GetCursorScreenPos();
        ImVec2 max(min.x + width, min.y + kTileHeight);
        ImGui::SetNextItemAllowOverlap();
        bool released = ImGui::InvisibleButton("##Tile", ImVec2(width, kTileHeight));
        bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem);
        bool visible = ImGui::IsItemVisible();
        bool dropping = false;
        HandleItem(item, released);
        if (item.IsFolder && !IsReadOnly(item.Path)) dropping = AcceptMove(item.Path);
        bool selected = IsSelected(item);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (dropping || selected) {
            draw->AddRectFilled(min, max,
                                ToImColor(m_Focused || dropping ? Colors::AccentSoft : Colors::SelectionUnfocused),
                                Sizes::RadiusMd);
            draw->AddRect(min, max, ToImColor(Colors::Accent), Sizes::RadiusMd);
        } else if (hovered) {
            draw->AddRectFilled(min, max, ToImColor(Colors::BgHover), Sizes::RadiusMd);
        }

        // The thumbnail: the texture's own image, or the type's icon.
        ImVec2 thumbMin(std::floor(min.x + (width - kThumbSize) * 0.5f), min.y + Sizes::Space2);
        ImVec2 thumbMax(thumbMin.x + kThumbSize, thumbMin.y + kThumbSize);
        u64 image = !item.IsFolder && item.Type == Assets::AssetType::Texture ? GetThumbnail(item.ID, visible) : 0;
        if (!item.IsFolder) draw->AddRectFilled(thumbMin, thumbMax, ToImColor(Colors::BgField), Sizes::RadiusSm);
        if (image) {
            draw->AddImageRounded(image, thumbMin, thumbMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), IM_COL32_WHITE,
                                  Sizes::RadiusSm);
        } else {
            ImFont* icons = GetFont(TextStyle::Icons32);
            const char* icon = item.IsFolder ? Icons::Folder : GetTypeIcon(item.Type);
            ImVec2 iconSize = icons->CalcTextSizeA(icons->FontSize, FLT_MAX, 0.0f, icon);
            draw->AddText(icons, icons->FontSize,
                          ImVec2(std::floor(thumbMin.x + (kThumbSize - iconSize.x) * 0.5f),
                                 std::floor(thumbMin.y + (kThumbSize - iconSize.y) * 0.5f)),
                          ToImColor(item.IsSubAsset ? Colors::TextFaint : Colors::TextMuted), icon);
        }

        // A model's badge opens and closes it, to show what is inside.
        if (!item.IsFolder && !item.IsSubAsset && item.Type == Assets::AssetType::Model) {
            bool open = m_OpenModels.count(item.ID) != 0;
            ImFont* small = GetFont(TextStyle::UISmall);
            const char* badgeIcon = open ? Icons::ChevronLeft : Icons::Layers;
            ImVec2 badgeMax(thumbMax.x - 3.0f, thumbMax.y - 3.0f);
            ImVec2 badgeMin(badgeMax.x - 16.0f, badgeMax.y - 16.0f);
            ImGui::SetCursorScreenPos(badgeMin);
            if (ImGui::InvisibleButton("##Open", ImVec2(16.0f, 16.0f))) {
                if (open) {
                    m_OpenModels.erase(item.ID);
                } else {
                    m_OpenModels.insert(item.ID);
                    // A model never loaded has no list of what is inside yet.
                    if (m_Scene.GetAssets().GetSubAssets(item.ID).empty()) m_Scene.GetAssets().LoadModel(item.ID);
                }
                Refresh();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(open ? "Hide the meshes and materials" : "Show the meshes and materials");
            draw->AddRectFilled(badgeMin, badgeMax,
                                ToImColor(ImGui::IsItemHovered() ? Colors::BgHover : Colors::BgRaised), 2.0f);
            ImVec2 iconSize = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, badgeIcon);
            draw->AddText(small, small->FontSize,
                          ImVec2(std::floor(badgeMin.x + (16.0f - iconSize.x) * 0.5f),
                                 std::floor(badgeMin.y + (16.0f - iconSize.y) * 0.5f)),
                          ToImColor(Colors::TextMuted), badgeIcon);
        }

        // The name, or the rename field over it.
        f32 labelTop = thumbMax.y + Sizes::Space2;
        if (m_Renaming == item.Path && !item.IsSubAsset) {
            ImGui::SetCursorScreenPos(ImVec2(min.x + Sizes::Space1, labelTop));
            ImGui::SetNextItemWidth(width - 2.0f * Sizes::Space1);
            if (m_FocusRename) {
                ImGui::SetKeyboardFocusHere();
                m_FocusRename = false;
            }
            ImGui::InputText("##Rename", &m_RenameText, ImGuiInputTextFlags_AutoSelectAll);
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                m_Renaming.clear();
            } else if (ImGui::IsItemDeactivated()) {
                FinishRename();
            }
        } else {
            DrawTwoLines(GetFont(TextStyle::UIMedium), item.Name, min.x + width * 0.5f, labelTop,
                         width - 2.0f * Sizes::Space1, item.IsSubAsset ? Colors::TextMuted : Colors::Text);
        }
        if (!item.IsFolder) {
            ImFont* small = GetFont(TextStyle::UISmall);
            const char* kind = GetTypeName(item.Type);
            f32 kindWidth = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, kind).x;
            draw->AddText(small, small->FontSize,
                          ImVec2(std::floor(min.x + (width - kindWidth) * 0.5f), labelTop + 28.0f + Sizes::Space1),
                          ToImColor(Colors::TextMuted), kind);
        }

        ImGui::SetCursorScreenPos(min);
        ImGui::Dummy(ImVec2(width, kTileHeight));
        ImGui::PopID();
    }

    void AssetBrowserPanel::DrawListRow(const Item& item) {
        ImGui::PushID(item.IsSubAsset ? item.ID.ToString().c_str() : item.Path.generic_u8string().c_str());
        ImVec2 min = ImGui::GetCursorScreenPos();
        f32 width = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
        ImVec2 max(min.x + width, min.y + Sizes::RowHeight);
        ImGui::SetNextItemAllowOverlap();
        bool released = ImGui::InvisibleButton("##Row", ImVec2(width, Sizes::RowHeight));
        bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem);
        HandleItem(item, released);
        bool dropping = item.IsFolder && !IsReadOnly(item.Path) && AcceptMove(item.Path);
        bool selected = IsSelected(item);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (dropping) {
            draw->AddRectFilled(min, max, ToImColor(Colors::AccentSoft));
            draw->AddRect(min, max, ToImColor(Colors::Accent));
        } else if (selected) {
            draw->AddRectFilled(min, max, ToImColor(m_Focused ? Colors::AccentSoft : Colors::SelectionUnfocused));
        } else if (hovered) {
            draw->AddRectFilled(min, max, ToImColor(Colors::BgHover));
        }

        ImFont* font = GetFont(TextStyle::UI);
        f32 textY = min.y + std::floor((Sizes::RowHeight - font->FontSize) * 0.5f);
        f32 x = min.x + Sizes::Space4 + (item.IsSubAsset ? Sizes::Indent : 0.0f);
        if (!item.IsFolder && !item.IsSubAsset && item.Type == Assets::AssetType::Model) {
            bool open = m_OpenModels.count(item.ID) != 0;
            ImGui::SetCursorScreenPos(ImVec2(x, min.y));
            if (ImGui::InvisibleButton("##Open", ImVec2(kIconWidth, Sizes::RowHeight))) {
                if (open) {
                    m_OpenModels.erase(item.ID);
                } else {
                    m_OpenModels.insert(item.ID);
                    if (m_Scene.GetAssets().GetSubAssets(item.ID).empty()) m_Scene.GetAssets().LoadModel(item.ID);
                }
                Refresh();
            }
            draw->AddText(font, font->FontSize, ImVec2(x, textY),
                          ToImColor(ImGui::IsItemHovered() ? Colors::Text : Colors::TextMuted),
                          open ? Icons::ChevronDown : Icons::ChevronRight);
        }
        x += kIconWidth + Sizes::Space2;
        draw->AddText(font, font->FontSize, ImVec2(x, textY), ToImColor(Colors::TextMuted),
                      item.IsFolder ? Icons::Folder : GetTypeIcon(item.Type));
        x += kIconWidth + Sizes::Space3;

        ImFont* small = GetFont(TextStyle::UISmall);
        const char* kind = item.IsFolder ? "" : GetTypeName(item.Type);
        f32 kindWidth = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, kind).x;
        f32 kindX = max.x - Sizes::Space4 - kindWidth;
        draw->AddText(small, small->FontSize,
                      ImVec2(kindX, min.y + std::floor((Sizes::RowHeight - small->FontSize) * 0.5f)),
                      ToImColor(Colors::TextMuted), kind);

        if (m_Renaming == item.Path && !item.IsSubAsset) {
            ImGui::SetCursorScreenPos(ImVec2(x - Sizes::Space2, min.y));
            ImGui::SetNextItemWidth(kindX - x - Sizes::Space4);
            if (m_FocusRename) {
                ImGui::SetKeyboardFocusHere();
                m_FocusRename = false;
            }
            ImGui::InputText("##Rename", &m_RenameText, ImGuiInputTextFlags_AutoSelectAll);
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                m_Renaming.clear();
            } else if (ImGui::IsItemDeactivated()) {
                FinishRename();
            }
        } else {
            ImVec4 clip(x, min.y, kindX - Sizes::Space4, max.y);
            draw->AddText(font, font->FontSize, ImVec2(x, textY),
                          ToImColor(item.IsSubAsset ? Colors::TextMuted : Colors::Text), item.Name.c_str(), nullptr,
                          0.0f, &clip);
        }

        ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        ImGui::PopID();
    }

    void AssetBrowserPanel::HandleItem(const Item& item, bool released) {
        // Selected on release, so dragging an asset onto a field of the selected one keeps it in the Inspector.
        if (released || ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            m_SelectedPath = item.Path;
            m_SelectedSubAsset = item.IsSubAsset ? item.ID : Assets::AssetID{};
            if (item.IsFolder) {
                if (m_Scene.GetSelectedAsset().IsValid()) m_Scene.SelectAsset({});
            } else {
                m_Scene.SelectAsset(item.ID);
            }
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (item.IsFolder) {
                m_Folder = item.Path;
                m_Filter.clear();
            } else if (!item.IsSubAsset && item.Type == Assets::AssetType::Scene) {
                m_SceneToOpen = item.ID;
            } else if (!item.IsSubAsset && item.Type == Assets::AssetType::Prefab) {
                m_Scene.AskToOpenPrefab(item.ID);
            } else if (!item.IsSubAsset && item.Type == Assets::AssetType::Model) {
                if (!m_OpenModels.erase(item.ID)) {
                    m_OpenModels.insert(item.ID);
                    if (m_Scene.GetAssets().GetSubAssets(item.ID).empty()) m_Scene.GetAssets().LoadModel(item.ID);
                }
                Refresh();
            }
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("##ItemMenu");
        if (ImGui::BeginPopup("##ItemMenu")) {
            DrawItemMenu(item);
            ImGui::EndPopup();
        }

        // Assets drag their ID, to asset fields, the Hierarchy and the Viewport; folders their path.
        if (m_Renaming != item.Path && ImGui::BeginDragDropSource()) {
            if (item.IsFolder) {
                std::string path = item.Path.u8string();
                ImGui::SetDragDropPayload(kFolderPayload, path.c_str(), path.size() + 1);
            } else {
                ImGui::SetDragDropPayload(Fields::kAssetPayload, &item.ID, sizeof(item.ID));
            }
            ImGui::Text("%s  %s", item.IsFolder ? Icons::Folder : GetTypeIcon(item.Type), item.Name.c_str());
            ImGui::EndDragDropSource();
        }
    }

    void AssetBrowserPanel::DrawItemMenu(const Item& item) {
        bool writable = !item.IsSubAsset && !IsReadOnly(item.Path);
        if (item.IsFolder && ImGui::MenuItem(WithIcon(Icons::FolderOpen, "Open").c_str())) m_Folder = item.Path;
        if (!item.IsSubAsset && item.Type == Assets::AssetType::Scene && !item.IsFolder &&
            ImGui::MenuItem(WithIcon(Icons::Clapperboard, "Open scene").c_str())) {
            m_SceneToOpen = item.ID;
        }
        if (!item.IsSubAsset && item.Type == Assets::AssetType::Prefab && !item.IsFolder &&
            ImGui::MenuItem(WithIcon(Icons::Package, "Open prefab").c_str())) {
            m_Scene.AskToOpenPrefab(item.ID);
        }
        if (ImGui::MenuItem(WithIcon(Icons::Copy, "Copy ID").c_str(), nullptr, false, !item.IsFolder)) {
            ImGui::SetClipboardText(item.ID.ToString().c_str());
        }
        ImGui::Separator();
        if (ImGui::MenuItem(WithIcon(Icons::Pencil, "Rename").c_str(), "F2", false, writable)) StartRename(item);
        if (ImGui::MenuItem(WithIcon(Icons::Trash2, "Delete").c_str(), "Delete", false, writable)) {
            m_ToDelete = item.Path;
            m_OpenDeleteDialog = true;
        }
    }

    void AssetBrowserPanel::DrawEmptyMenu() {
        bool writable = !IsReadOnly(m_Folder);
        if (ImGui::MenuItem(WithIcon(Icons::FolderPlus, "New folder").c_str(), nullptr, false, writable)) {
            CreateFolder();
        }
        if (ImGui::MenuItem(WithIcon(Icons::Palette, "New material").c_str(), nullptr, false, writable)) {
            CreateMaterial();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(WithIcon(Icons::RefreshCw, "Refresh").c_str())) {
            m_Scene.GetAssets().Refresh();
            m_Edits.DropMissing();
            Refresh();
        }
    }

    void AssetBrowserPanel::HandleShortcuts() {
        if (!m_Focused || !m_Renaming.empty() || ImGui::GetIO().WantTextInput) return;
        if (m_SelectedPath.empty() || m_SelectedSubAsset.IsValid() || IsReadOnly(m_SelectedPath)) return;
        if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
            for (const Item& item : m_Items) {
                if (item.Path == m_SelectedPath && !item.IsSubAsset) StartRename(item);
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            m_ToDelete = m_SelectedPath;
            m_OpenDeleteDialog = true;
        }
    }

    bool AssetBrowserPanel::AcceptMove(const std::filesystem::path& folder) {
        bool over = false;
        if (!ImGui::BeginDragDropTarget()) return false;
        ImGuiDragDropFlags flags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
        Assets::AssetManager& assets = m_Scene.GetAssets();
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(Fields::kAssetPayload, flags)) {
            Assets::AssetID id = *static_cast<const Assets::AssetID*>(payload->Data);
            std::filesystem::path file = assets.GetFile(id);
            // Only asset files can move, and not into the folder they are in.
            over = !file.empty() && assets.IsWritable(id) && file.parent_path() != folder;
            if (over && payload->IsDelivery()) Move(file, folder);
        } else if (const ImGuiPayload* folderPayload = ImGui::AcceptDragDropPayload(kFolderPayload, flags)) {
            std::filesystem::path dragged = std::filesystem::u8path(static_cast<const char*>(folderPayload->Data));
            // Not into itself or a folder inside it.
            std::string relative = folder.lexically_relative(dragged).generic_string();
            bool inside = relative == "." || (!relative.empty() && relative.rfind("..", 0) != 0);
            over = !inside && !IsReadOnly(dragged) && dragged.parent_path() != folder;
            if (over && folderPayload->IsDelivery()) Move(dragged, folder);
        }
        ImGui::EndDragDropTarget();
        return AcceptObject(folder) || over;
    }

    bool AssetBrowserPanel::AcceptObject(const std::filesystem::path& folder) {
        bool over = false;
        if (!ImGui::BeginDragDropTarget()) return false;
        ImGuiDragDropFlags flags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(Fields::kObjectPayload, flags)) {
            auto handle = *static_cast<const Handle<Scene::GameObject>*>(payload->Data);
            Scene::GameObject* object = m_Scene.GetScene().Get(handle);
            over = object && m_Scene.CanCreatePrefab(*object);
            if (over && payload->IsDelivery()) CreatePrefab(*object, folder);
        }
        ImGui::EndDragDropTarget();
        return over;
    }

    void AssetBrowserPanel::CreatePrefab(Scene::GameObject& object, const std::filesystem::path& folder) {
        // Named after the object, with what a file name can't have replaced.
        std::string name = object.GetName();
        for (char& c : name) {
            if (std::strchr("\\/:*?\"<>|", c)) c = '_';
        }
        std::filesystem::path file = MakeUniquePath(folder, name.empty() ? "Prefab" : name, ".prefab");
        Assets::AssetID prefab = m_Scene.CreatePrefab(object, file);
        if (!prefab.IsValid()) return;
        Refresh();
        m_SelectedPath = file;
    }

    void AssetBrowserPanel::Move(const std::filesystem::path& from, const std::filesystem::path& folder) {
        std::filesystem::path to = folder / from.filename();
        if (!m_Scene.GetAssets().Move(from, to)) return;
        VKE_CORE_INFO("Editor: moved {} to {}", from.filename().u8string(), folder.filename().u8string());
        if (m_SelectedPath == from) m_SelectedPath = to;
        // The open folder moved along, or was inside the moved one.
        std::string relative = m_Folder.lexically_relative(from).generic_string();
        if (relative == ".") {
            m_Folder = to;
        } else if (!relative.empty() && relative.rfind("..", 0) != 0) {
            m_Folder = to / std::filesystem::u8path(relative);
        }
        Refresh();
    }

    void AssetBrowserPanel::CreateFolder() {
        std::filesystem::path folder = MakeUniquePath(m_Folder, "New Folder", "");
        std::error_code ec;
        if (!std::filesystem::create_directory(folder, ec)) {
            VKE_CORE_ERROR("Editor: couldn't create the folder '{}': {}", folder.u8string(), ec.message());
            return;
        }
        Refresh();
        UpdateListing();
        m_SelectedPath = folder;
        Item item;
        item.Path = folder;
        item.Name = folder.filename().u8string();
        item.IsFolder = true;
        StartRename(item);
    }

    void AssetBrowserPanel::CreateMaterial() {
        std::filesystem::path file = MakeUniquePath(m_Folder, "New Material", ".mat");
        Assets::AssetID id = m_Scene.GetAssets().CreateMaterial(file);
        if (!id.IsValid()) return;
        Refresh();
        UpdateListing();
        m_SelectedPath = file;
        m_SelectedSubAsset = {};
        m_Scene.SelectAsset(id);
        Item item;
        item.Path = file;
        item.Name = file.stem().u8string();
        item.ID = id;
        item.Type = Assets::AssetType::Material;
        StartRename(item);
    }

    void AssetBrowserPanel::StartRename(const Item& item) {
        m_Renaming = item.Path;
        m_RenameText = item.Name;
        m_FocusRename = true;
    }

    void AssetBrowserPanel::FinishRename() {
        std::filesystem::path from = m_Renaming;
        m_Renaming.clear();
        std::string name = m_RenameText;
        name.erase(0, name.find_first_not_of(' '));
        name.erase(name.find_last_not_of(' ') + 1);
        if (name.empty() || name.find_first_of("\\/:*?\"<>|") != std::string::npos) {
            if (!name.empty()) VKE_CORE_ERROR("Editor: '{}' isn't a valid file name", name);
            return;
        }
        // Files keep their extension: it says what kind of asset they are.
        std::filesystem::path to =
            from.parent_path() / std::filesystem::u8path(IsFolder(from) ? name : name + from.extension().u8string());
        if (to == from) return;
        if (!m_Scene.GetAssets().Move(from, to)) return;
        if (m_SelectedPath == from) m_SelectedPath = to;
        Refresh();
    }

    void AssetBrowserPanel::DrawDeleteDialog() {
        const char* id = "Delete##AssetDialog";
        if (m_OpenDeleteDialog) {
            ImGui::OpenPopup(id);
            m_OpenDeleteDialog = false;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                       viewport->WorkPos.y + viewport->WorkSize.y * 0.35f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f));
        Widgets::PushDialogStyle();
        bool open = ImGui::BeginPopupModal(id, nullptr,
                                           ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        Widgets::PopDialogStyle();
        if (!open) return;

        bool folder = IsFolder(m_ToDelete);
        std::string title = "Delete " + m_ToDelete.filename().u8string() + "?";
        bool cancel = Widgets::DialogHeader(title.c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape);
        ImGui::PushTextWrapPos(0.0f);
        Widgets::Text(folder ? "The folder and every file in it are deleted from the disk. This can't be undone."
                             : "The file is deleted from the disk. Scenes and materials that use it lose it. This "
                               "can't be undone.",
                      Colors::TextMuted);
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space5));

        ImGui::PushFont(GetFont(TextStyle::UIMedium));
        const ImGuiStyle& style = ImGui::GetStyle();
        f32 cancelWidth = ImGui::CalcTextSize("Cancel").x + 2.0f * style.FramePadding.x;
        f32 deleteWidth = ImGui::CalcTextSize("Delete").x + 2.0f * style.FramePadding.x;
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - cancelWidth - deleteWidth - Sizes::Space4);
        cancel |= ImGui::Button("Cancel");
        ImGui::PopFont();
        ImGui::SameLine(0.0f, Sizes::Space4);
        bool confirm = Widgets::PrimaryButton("Delete");

        if (cancel || confirm) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();

        if (!confirm) return;
        Assets::AssetManager& assets = m_Scene.GetAssets();
        Assets::AssetID selected = m_Scene.GetSelectedAsset();
        if (!assets.Delete(m_ToDelete)) return;
        VKE_CORE_INFO("Editor: deleted {}", m_ToDelete.filename().u8string());
        if (selected.IsValid() && !assets.GetAnyType(selected)) m_Scene.SelectAsset({});
        if (m_SelectedPath == m_ToDelete) m_SelectedPath.clear();
        m_Edits.DropMissing();
        Refresh();
    }

    u64 AssetBrowserPanel::GetThumbnail(Assets::AssetID texture, bool visible) {
        Assets::AssetManager& assets = m_Scene.GetAssets();
        Handle<Assets::TextureData> handle = assets.FindLoadedTexture(texture);
        if (!handle.IsValid()) {
            // Loading reads and decodes the image, so the tiles in view take turns.
            if (m_LoadedThumbnail || !visible) return 0;
            m_LoadedThumbnail = true;
            Assets::ColorSpace space = assets.GetColorSpace(texture) == Assets::TextureColorSpace::Linear
                                           ? Assets::ColorSpace::Linear
                                           : Assets::ColorSpace::SRGB;
            handle = assets.LoadTexture(texture, space);
            if (!handle.IsValid()) return 0;
        }
        return m_UI.GetTextureID(Renderer::OverlayTexture::FromAsset(handle));
    }

} // namespace VKE::Editor
