#include "HierarchyPanel.h"
#include "Display.h"
#include "EditorScene.h"
#include "Fields.h"
#include "ObjectMenus.h"
#include "Widgets.h"

#include <assets/AssetManager.h>
#include <core/Log.h>
#include <scene/GameObject.h>
#include <scene/Scene.h>
#include <ui/Icons.h>
#include <ui/Style.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>

namespace VKE::Editor {

    using UI::GetFont;
    using UI::TextStyle;
    using UI::ToImColor;
    namespace Colors = UI::Colors;
    namespace Icons = UI::Icons;
    namespace Sizes = UI::Sizes;

    namespace {
        // The chevron's and the icon's columns.
        constexpr f32 kIconWidth = 14.0f;

        std::string WithIcon(const char* icon, const char* label) {
            return std::string(icon) + "  " + label;
        }

        bool ContainsIgnoringCase(const std::string& text, const std::string& part) {
            auto it = std::search(text.begin(), text.end(), part.begin(), part.end(), [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });
            return it != text.end();
        }

        void CollectMatches(Scene::GameObject& object, const std::string& filter,
                            std::vector<Scene::GameObject*>& out) {
            if (ContainsIgnoringCase(object.GetName(), filter)) out.push_back(&object);
            for (Scene::GameObject* child : object.GetChildren()) {
                CollectMatches(*child, filter, out);
            }
        }

        // Text or an icon in the UI font, centered in a row that starts at `top`.
        void DrawRowText(f32 x, f32 top, u32 color, const char* text, f32 clipX = FLT_MAX) {
            ImFont* font = GetFont(TextStyle::UI);
            f32 y = top + std::floor((Sizes::RowHeight - font->FontSize) * 0.5f);
            ImVec4 clip(x, top, clipX, top + Sizes::RowHeight);
            ImGui::GetWindowDrawList()->AddText(font, font->FontSize, ImVec2(x, y), ToImColor(color), text, nullptr,
                                                0.0f, &clip);
        }
    } // namespace

    HierarchyPanel::HierarchyPanel(EditorScene& scene) : m_Scene(scene) {
    }

    const char* HierarchyPanel::GetWindowName() {
        static const std::string name = std::string(Icons::ListTree) + "  Hierarchy###Hierarchy";
        return name.c_str();
    }

    void HierarchyPanel::Draw(bool* open) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        bool visible = ImGui::Begin(GetWindowName(), open);
        ImGui::PopStyleVar();
        if (visible) {
            m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

            // Selected from elsewhere, like the GameObject menu: show its row.
            Scene::GameObject* selected = m_Scene.GetSelected();
            Handle<Scene::GameObject> selection = selected ? selected->GetHandle() : Handle<Scene::GameObject>();
            if (selection != m_LastSelected) {
                m_LastSelected = selection;
                if (selected) {
                    Reveal(*selected);
                    m_ScrollToSelected = true;
                }
            }

            DrawToolbar();
            DrawTree();
            HandleShortcuts();
        }
        ImGui::End();

        Scene::Scene& scene = m_Scene.GetScene();
        if (m_DropPending) {
            m_DropPending = false;
            Scene::GameObject* dragged = scene.Get(m_Dragged);
            Scene::GameObject* target = scene.Get(m_DropTarget);
            if (dragged && (target || !m_DropTarget.IsValid())) Move(*dragged, target, m_DropPlace);
        }
        if (m_AssetToPlace.IsValid()) {
            Scene::GameObject* parent = scene.Get(m_PlaceParent);
            if (parent || !m_PlaceParent.IsValid()) {
                if (Scene::GameObject* placed = m_Scene.Place(m_AssetToPlace, parent)) m_Scene.Select(placed);
            }
            m_AssetToPlace = {};
        }
        if (Scene::GameObject* doomed = scene.Get(m_ToDestroy)) {
            VKE_CORE_INFO("Editor: destroyed '{}'", doomed->GetName());
            m_Scene.Destroy(*doomed);
        }
        m_ToDestroy = {};
        if (Scene::GameObject* instance = scene.Get(m_ToUnpack)) m_Scene.Unpack(*instance);
        m_ToUnpack = {};
    }

    void HierarchyPanel::DrawToolbar() {
        // Below the tab bar, which is part of the window.
        ImGui::SetCursorPos(ImVec2(Sizes::Space4, ImGui::GetCursorPosY() + Sizes::Space3));
        if (ImGui::Button(Icons::Plus, ImVec2(26.0f, Sizes::ControlHeight))) ImGui::OpenPopup("##Create");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Create object");
        if (ImGui::BeginPopup("##Create")) {
            DrawCreateObjectItems(m_Scene, nullptr);
            ImGui::EndPopup();
        }
        ImGui::SameLine(0.0f, Sizes::Space3);
        Widgets::SearchField("##Filter", m_Filter, "Filter objects", ImGui::GetContentRegionAvail().x - Sizes::Space4);
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space1));
    }

    void HierarchyPanel::DrawTree() {
        ImGui::BeginChild("##Tree");
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space2));

        // Copies: the menus can add objects while the rows are drawn.
        if (m_Filter.empty()) {
            std::vector<Scene::GameObject*> roots = m_Scene.GetScene().GetRootObjects();
            for (Scene::GameObject* root : roots) {
                DrawRow(*root, 0, true);
            }
        } else {
            std::vector<Scene::GameObject*> matches;
            for (Scene::GameObject* root : m_Scene.GetScene().GetRootObjects()) {
                CollectMatches(*root, m_Filter, matches);
            }
            for (Scene::GameObject* match : matches) {
                DrawRow(*match, 0, false);
            }
        }

        // The space under the rows: a click deselects, a drop makes a root, the menu creates one.
        ImVec2 rest = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton("##Empty", ImVec2(std::max(rest.x, 1.0f), std::max(rest.y, Sizes::RowHeight)));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) m_Scene.Select(nullptr);
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(Fields::kObjectPayload)) {
                m_Dragged = *static_cast<const Handle<Scene::GameObject>*>(payload->Data);
                m_DropTarget = {};
                m_DropPending = true;
            } else if (const ImGuiPayload* asset =
                           ImGui::AcceptDragDropPayload(Fields::kAssetPayload, ImGuiDragDropFlags_AcceptBeforeDelivery)) {
                Assets::AssetID dropped = *static_cast<const Assets::AssetID*>(asset->Data);
                if (m_Scene.CanPlace(dropped) && asset->IsDelivery()) {
                    m_AssetToPlace = dropped;
                    m_PlaceParent = {};
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem("##EmptyMenu")) {
            DrawCreateObjectItems(m_Scene, nullptr);
            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();
        ImGui::EndChild();
    }

    void HierarchyPanel::DrawRow(Scene::GameObject& object, u32 depth, bool canDrag) {
        Scene::Scene& scene = m_Scene.GetScene();
        Handle<Scene::GameObject> handle = object.GetHandle();
        bool hasChildren = canDrag && !object.GetChildren().empty();
        bool open = hasChildren && m_Open.count(object.GetID()) != 0;
        bool selected = &object == m_Scene.GetSelected();
        bool renaming = m_Renaming == handle;

        ImGui::PushID(&object);
        ImVec2 min = ImGui::GetCursorScreenPos();
        f32 width = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
        ImVec2 max(min.x + width, min.y + Sizes::RowHeight);
        f32 x = min.x + Sizes::Space4 + static_cast<f32>(depth) * Sizes::Indent;

        ImGui::SetNextItemAllowOverlap();
        bool released = ImGui::InvisibleButton("##Row", ImVec2(width, Sizes::RowHeight));
        // Still hovered under the chevron and the eye, which sit on top of it, and while they are
        // held: the eye is only drawn while the row is hovered.
        bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem |
                                            ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        // Selected on release, so dragging an object onto a field keeps the Inspector where it is.
        if (released || ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            m_Scene.Select(&object);
            m_LastSelected = handle;
            selected = true;
        }
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) StartRename(object);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("##RowMenu");
        if (selected && m_ScrollToSelected) {
            ImGui::SetScrollHereY(0.5f);
            m_ScrollToSelected = false;
        }

        if (canDrag && !renaming && ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(Fields::kObjectPayload, &handle, sizeof(handle));
            ImGui::Text("%s  %s", Display::GetObjectIcon(object), object.GetName().c_str());
            ImGui::EndDragDropSource();
        }

        // Dropping on the top or bottom quarter of a row puts the object before or after it,
        // anywhere else inside it.
        bool dropping = false;
        DropPlace place = DropPlace::Inside;
        if (canDrag && ImGui::BeginDragDropTarget()) {
            ImGuiDragDropFlags flags =
                ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(Fields::kObjectPayload, flags)) {
                Handle<Scene::GameObject> draggedHandle = *static_cast<const Handle<Scene::GameObject>*>(payload->Data);
                Scene::GameObject* dragged = scene.Get(draggedHandle);
                f32 y = ImGui::GetMousePos().y - min.y;
                place = y < Sizes::RowHeight * 0.25f   ? DropPlace::Before
                        : y > Sizes::RowHeight * 0.75f ? DropPlace::After
                                                       : DropPlace::Inside;
                dropping = dragged && dragged != &object && !object.IsChildOf(dragged) && !m_Scene.IsFixed(*dragged) &&
                           m_Scene.AcceptsChildren(place == DropPlace::Inside ? &object : object.GetParent());
                if (dropping && payload->IsDelivery()) {
                    m_Dragged = draggedHandle;
                    m_DropTarget = handle;
                    m_DropPlace = place;
                    m_DropPending = true;
                }
            } else if (const ImGuiPayload* asset = ImGui::AcceptDragDropPayload(Fields::kAssetPayload, flags)) {
                // Goes inside the row's object.
                Assets::AssetID dropped = *static_cast<const Assets::AssetID*>(asset->Data);
                dropping = m_Scene.CanPlace(dropped) && m_Scene.AcceptsChildren(&object);
                if (dropping && asset->IsDelivery()) {
                    m_AssetToPlace = dropped;
                    m_PlaceParent = handle;
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopup("##RowMenu")) {
            DrawRowMenu(object);
            ImGui::EndPopup();
        }

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (dropping && place == DropPlace::Inside) {
            draw->AddRectFilled(min, max, ToImColor(Colors::AccentSoft));
            draw->AddRect(min, max, ToImColor(Colors::Accent));
        } else if (selected) {
            draw->AddRectFilled(min, max, ToImColor(m_Focused ? Colors::AccentSoft : Colors::SelectionUnfocused));
        } else if (hovered) {
            draw->AddRectFilled(min, max, ToImColor(Colors::BgHover));
        }
        if (dropping && place != DropPlace::Inside) {
            f32 lineY = place == DropPlace::Before ? min.y : max.y - 2.0f;
            draw->AddRectFilled(ImVec2(x, lineY), ImVec2(max.x, lineY + 2.0f), ToImColor(Colors::Accent));
        }

        if (hasChildren) {
            ImGui::SetCursorScreenPos(ImVec2(x, min.y));
            if (ImGui::InvisibleButton("##Chevron", ImVec2(kIconWidth, Sizes::RowHeight))) {
                if (open) {
                    m_Open.erase(object.GetID());
                } else {
                    m_Open.insert(object.GetID());
                }
                open = !open;
            }
            DrawRowText(x, min.y, ImGui::IsItemHovered() ? Colors::Text : Colors::TextMuted,
                        open ? Icons::ChevronDown : Icons::ChevronRight);
        }

        // Inactive objects, and everything under them, are faint.
        bool active = object.IsActiveInHierarchy();
        f32 iconX = x + kIconWidth + Sizes::Space3;
        bool isInstance = object.IsPrefabPart();
        DrawRowText(iconX, min.y,
                    !active      ? Colors::TextFaint
                    : isInstance ? Colors::Info
                    : selected   ? Colors::Text
                                 : Colors::TextMuted,
                    object.IsPrefabRoot() ? Icons::Package : Display::GetObjectIcon(object));

        // The eye: always shown on an object turned off, on hover for the others.
        f32 eyeX = max.x - Sizes::Space4 - kIconWidth;
        if (!object.IsActive() || hovered) {
            ImGui::SetCursorScreenPos(ImVec2(eyeX, min.y));
            if (ImGui::InvisibleButton("##Eye", ImVec2(kIconWidth, Sizes::RowHeight))) {
                object.SetActive(!object.IsActive());
                m_Scene.MarkDirty();
            }
            bool eyeHovered = ImGui::IsItemHovered();
            if (eyeHovered) ImGui::SetTooltip(object.IsActive() ? "Turn off" : "Turn on");
            DrawRowText(eyeX, min.y, eyeHovered ? Colors::Text : Colors::TextFaint,
                        object.IsActive() ? Icons::Eye : Icons::EyeOff);
        }

        f32 nameX = iconX + kIconWidth + Sizes::Space3;
        if (renaming) {
            constexpr f32 kFieldHeight = 20.0f;
            ImGui::SetCursorScreenPos(ImVec2(nameX, min.y + std::floor((Sizes::RowHeight - kFieldHeight) * 0.5f)));
            ImGui::SetNextItemWidth(std::max(eyeX - Sizes::Space3 - nameX, 40.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                                ImVec2(Sizes::Space3, std::floor((kFieldHeight - ImGui::GetFontSize()) * 0.5f)));
            ImGui::PushStyleColor(ImGuiCol_Border, Widgets::ToVec4(Colors::Focus));
            if (m_FocusRename) {
                ImGui::SetKeyboardFocusHere();
                m_FocusRename = false;
            }
            bool entered = ImGui::InputText("##Rename", &m_RenameText,
                                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            bool cancelled = ImGui::IsKeyPressed(ImGuiKey_Escape);
            // Done on Enter, Escape or a click elsewhere, once the field has had the focus.
            if (entered || ImGui::IsItemDeactivated()) {
                std::string name = m_RenameText;
                name.erase(0, name.find_first_not_of(' '));
                name.erase(name.find_last_not_of(' ') + 1);
                if (!cancelled && !name.empty() && name != object.GetName()) {
                    object.SetName(name);
                    m_Scene.MarkDirty();
                }
                m_Renaming = {};
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        } else {
            u32 color = !active ? Colors::TextFaint : isInstance ? Colors::Info : Colors::Text;
            DrawRowText(nameX, min.y, color, object.GetName().c_str(), eyeX - Sizes::Space3);
        }

        ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
        ImGui::PopID();

        if (open) {
            std::vector<Scene::GameObject*> children = object.GetChildren();
            for (Scene::GameObject* child : children) {
                DrawRow(*child, depth + 1, canDrag);
            }
        }
    }

    void HierarchyPanel::DrawRowMenu(Scene::GameObject& object) {
        if (ImGui::BeginMenu(WithIcon(Icons::Plus, "Create child").c_str(), m_Scene.AcceptsChildren(&object))) {
            DrawCreateObjectItems(m_Scene, &object);
            ImGui::EndMenu();
        }
        if (Scene::GameObject* instance = object.GetPrefabRoot()) {
            ImGui::Separator();
            if (ImGui::MenuItem(WithIcon(Icons::Package, "Open prefab").c_str())) {
                m_Scene.AskToOpenPrefab(instance->GetPrefab());
            }
            if (ImGui::MenuItem(WithIcon(Icons::Unlink, "Unpack prefab").c_str())) m_ToUnpack = instance->GetHandle();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(WithIcon(Icons::Pencil, "Rename").c_str(), "F2")) StartRename(object);
        if (ImGui::MenuItem(WithIcon(object.IsActive() ? Icons::EyeOff : Icons::Eye,
                                     object.IsActive() ? "Turn off" : "Turn on")
                                .c_str())) {
            object.SetActive(!object.IsActive());
            m_Scene.MarkDirty();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(WithIcon(Icons::Trash2, "Delete").c_str(), "Del", false, !m_Scene.IsFixed(object))) {
            m_ToDestroy = object.GetHandle();
        }
    }

    void HierarchyPanel::HandleShortcuts() {
        Scene::GameObject* selected = m_Scene.GetSelected();
        if (!m_Focused || !selected || ImGui::GetIO().WantTextInput) return;
        if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) StartRename(*selected);
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !m_Scene.IsFixed(*selected)) {
            m_ToDestroy = selected->GetHandle();
        }
    }

    void HierarchyPanel::Move(Scene::GameObject& dragged, Scene::GameObject* target, DropPlace place) {
        if (m_Scene.IsFixed(dragged)) return;
        if (!target) {
            dragged.SetParent(m_Scene.GetDefaultParent());
            dragged.SetIndexInParent(SIZE_MAX);
        } else if (place == DropPlace::Inside) {
            dragged.SetParent(target);
            dragged.SetIndexInParent(SIZE_MAX);
            m_Open.insert(target->GetID());
        } else {
            dragged.SetParent(target->GetParent());
            // The target moves up one place once the dragged object leaves from before it.
            size_t index = target->GetIndexInParent();
            if (dragged.GetIndexInParent() < index) --index;
            dragged.SetIndexInParent(place == DropPlace::Before ? index : index + 1);
        }
        m_Scene.MarkDirty();
    }

    void HierarchyPanel::StartRename(Scene::GameObject& object) {
        Reveal(object);
        m_Renaming = object.GetHandle();
        m_RenameText = object.GetName();
        m_FocusRename = true;
    }

    void HierarchyPanel::Reveal(const Scene::GameObject& object) {
        for (const Scene::GameObject* parent = object.GetParent(); parent; parent = parent->GetParent()) {
            m_Open.insert(parent->GetID());
        }
    }

} // namespace VKE::Editor
