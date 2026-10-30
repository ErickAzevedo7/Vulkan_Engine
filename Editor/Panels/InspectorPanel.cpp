#include "InspectorPanel.h"
#include "Display.h"
#include "EditorScene.h"
#include "Fields.h"
#include "MaterialEdits.h"
#include "Widgets.h"

#include <assets/AssetManager.h>
#include <renderer/Overlay.h>
#include <ui/UISystem.h>
#include <scene/Component.h>
#include <scene/GameObject.h>
#include <scene/Properties.h>
#include <scene/Scene.h>
#include <ui/Icons.h>
#include <ui/Style.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace VKE::Editor {

    using UI::GetFont;
    using UI::TextStyle;
    using UI::ToImColor;
    namespace Colors = UI::Colors;
    namespace Icons = UI::Icons;
    namespace Sizes = UI::Sizes;

    namespace {
        // Vector rows have a narrower label, so their three numbers have room.
        constexpr f32 kVectorLabelWidth = 64.0f;
        // The left edge of a component's rows, past its header's chevron.
        constexpr f32 kBodyIndent = 26.0f;
        constexpr f32 kAddButtonWidth = 240.0f;

        std::string WithIcon(const char* icon, const char* label) {
            return std::string(icon) + "  " + label;
        }

        bool ContainsIgnoringCase(const std::string& text, const std::string& part) {
            auto it = std::search(text.begin(), text.end(), part.begin(), part.end(), [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });
            return it != text.end();
        }

        bool IsClose(const Quat& a, const Quat& b) {
            constexpr f32 kEpsilon = 1e-5f;
            return std::abs(a.x - b.x) < kEpsilon && std::abs(a.y - b.y) < kEpsilon && std::abs(a.z - b.z) < kEpsilon &&
                   std::abs(a.w - b.w) < kEpsilon;
        }

        // Copies a property's value from one object of its type to another.
        void CopyValue(const Scene::PropertyInfo& property, void* to, const void* from) {
            using Scene::PropertyType;
            switch (property.Type) {
            case PropertyType::Bool: *static_cast<bool*>(to) = *static_cast<const bool*>(from); break;
            case PropertyType::F32: *static_cast<f32*>(to) = *static_cast<const f32*>(from); break;
            case PropertyType::Vec3: *static_cast<Vec3*>(to) = *static_cast<const Vec3*>(from); break;
            case PropertyType::Quat: *static_cast<Quat*>(to) = *static_cast<const Quat*>(from); break;
            case PropertyType::Enum: property.Enum->Write(to, property.Enum->Read(from)); break;
            case PropertyType::Struct:
                for (const Scene::PropertyInfo& inner : *property.Struct) {
                    CopyValue(inner, inner.Get(to), inner.Get(from));
                }
                break;
            case PropertyType::Mesh:
                *static_cast<Handle<Assets::MeshData>*>(to) = *static_cast<const Handle<Assets::MeshData>*>(from);
                break;
            case PropertyType::Materials:
                *static_cast<std::vector<Handle<Assets::MaterialData>>*>(to) =
                    *static_cast<const std::vector<Handle<Assets::MaterialData>>*>(from);
                break;
            }
        }

        // A row's height, centered text in the UI font.
        void RowText(const char* text, u32 color) {
            ImGui::AlignTextToFramePadding();
            Widgets::Text(text, color);
        }
    } // namespace

    InspectorPanel::InspectorPanel(EditorScene& scene, Assets::AssetManager& assets, MaterialEdits& edits,
                                   UI::UISystem& ui)
        : m_Scene(scene), m_Assets(assets), m_Edits(edits), m_UI(ui) {
    }

    const char* InspectorPanel::GetWindowName() {
        static const std::string name = std::string(Icons::SlidersHorizontal) + "  Inspector###Inspector";
        return name.c_str();
    }

    void InspectorPanel::Draw(bool* open) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        bool visible = ImGui::Begin(GetWindowName(), open);
        ImGui::PopStyleVar();
        if (!visible) {
            ImGui::End();
            return;
        }

        if (Assets::AssetID asset = m_Scene.GetSelectedAsset(); asset.IsValid()) {
            ImGui::BeginChild("##Asset");
            DrawAsset(asset);
            ImGui::EndChild();
            ImGui::End();
            return;
        }

        Scene::GameObject* object = m_Scene.GetSelected();
        if (!object) {
            const char* note = "Select an object or an asset to see it here.";
            ImVec2 size = ImGui::CalcTextSize(note);
            ImVec2 available = ImGui::GetContentRegionAvail();
            ImGui::SetCursorPos(ImVec2(std::max(Sizes::Space4, std::floor((available.x - size.x) * 0.5f)),
                                       ImGui::GetCursorPosY() + Sizes::Space7));
            Widgets::Text(note, Colors::TextMuted);
            ImGui::End();
            return;
        }

        if (m_Object != object->GetHandle()) {
            m_Object = object->GetHandle();
            m_AddFilter.clear();
        }

        ImGui::BeginChild("##Components");
        DrawObjectHeader(*object);

        // Copied: the add menu can add a component while the blocks are drawn.
        std::vector<Scene::Component*> components;
        for (Scene::Component* component : object->GetComponents()) {
            components.push_back(component);
        }
        for (Scene::Component* component : components) {
            DrawComponent(*component);
        }
        DrawAddComponent(*object);
        ImGui::EndChild();
        ImGui::End();

        if (m_ToReset != Scene::kInvalidComponentType) {
            const Scene::ComponentTypeInfo* type = Scene::ComponentRegistry::Find(m_ToReset);
            if (Scene::Component* component = object->GetComponent(m_ToReset); component && type) {
                void* target = type->ToObject(component);
                const void* defaults = type->ToObject(type->Defaults.get());
                for (const Scene::PropertyInfo& property : *type->Properties) {
                    CopyValue(property, property.Get(target), property.Get(defaults));
                }
                component->ApplyPropertyChanges(m_Assets);
                m_Scene.MarkDirty();
            }
            m_ToReset = Scene::kInvalidComponentType;
        }
        if (m_ToRemove != Scene::kInvalidComponentType) {
            if (object->RemoveComponent(m_ToRemove)) m_Scene.MarkDirty();
            m_ToRemove = Scene::kInvalidComponentType;
        }
    }

    void InspectorPanel::DrawObjectHeader(Scene::GameObject& object) {
        ImGui::SetCursorPos(ImVec2(Sizes::Space4, ImGui::GetCursorPosY() + Sizes::Space4));
        bool active = object.IsActive();
        if (Fields::Checkbox("##Active", active)) {
            object.SetActive(active);
            m_Scene.MarkDirty();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(active ? "On" : "Off");

        // The text follows the object unless it is being typed into.
        ImGui::SameLine(0.0f, Sizes::Space4);
        if (ImGui::GetActiveID() != ImGui::GetID("##Name")) m_NameText = object.GetName();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - Sizes::Space4);
        ImGui::InputText("##Name", &m_NameText);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            std::string name = m_NameText;
            name.erase(0, name.find_first_not_of(' '));
            name.erase(name.find_last_not_of(' ') + 1);
            if (!name.empty() && name != object.GetName()) {
                object.SetName(name);
                m_Scene.MarkDirty();
            }
        }

        // The ID under the name, and the component count on the right.
        ImFont* mono = GetFont(TextStyle::MonoSmall);
        std::string id = "ID " + object.GetID().ToString();
        size_t count = object.GetComponents().Size();
        std::string components = std::to_string(count) + (count == 1 ? " component" : " components");
        ImGui::SetCursorPosX(Sizes::Space4 + 14.0f + Sizes::Space4);
        ImGui::PushFont(mono);
        ImGui::PushStyleColor(ImGuiCol_Text, Widgets::ToVec4(Colors::TextMuted));
        ImGui::TextUnformatted(id.c_str());
        f32 right = ImGui::GetWindowContentRegionMax().x - Sizes::Space4 -
                    mono->CalcTextSizeA(mono->FontSize, FLT_MAX, 0.0f, components.c_str()).x;
        ImGui::SameLine(std::max(right, ImGui::GetCursorPosX()));
        ImGui::TextUnformatted(components.c_str());
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space3));
    }

    ImVec2 InspectorPanel::DrawBlockHeader(const char* icon, const std::string& label, bool& open) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 min = ImGui::GetCursorScreenPos();
        f32 width = ImGui::GetContentRegionAvail().x;
        ImVec2 max(min.x + width, min.y + Sizes::TabHeight);

        // A click opens or closes the block.
        ImGui::SetNextItemAllowOverlap();
        if (ImGui::InvisibleButton("##Header", ImVec2(width, Sizes::TabHeight))) open = !open;
        bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem);
        draw->AddRectFilled(min, max, ToImColor(hovered ? Colors::BgHover : Colors::BgHeader));
        draw->AddLine(min, ImVec2(max.x, min.y), ToImColor(Colors::Line));

        ImFont* font = GetFont(TextStyle::UI);
        ImFont* strong = GetFont(TextStyle::UIStrong);
        f32 x = min.x + Sizes::Space3;
        f32 textY = min.y + std::floor((Sizes::TabHeight - font->FontSize) * 0.5f);
        draw->AddText(font, font->FontSize, ImVec2(x, textY), ToImColor(Colors::TextMuted),
                      open ? Icons::ChevronDown : Icons::ChevronRight);
        x += 14.0f + Sizes::Space3;
        draw->AddText(font, font->FontSize, ImVec2(x, textY), ToImColor(Colors::TextMuted), icon);
        x += 14.0f + Sizes::Space3;
        f32 nameY = min.y + std::floor((Sizes::TabHeight - strong->FontSize) * 0.5f);
        draw->AddText(strong, strong->FontSize, ImVec2(x, nameY), ToImColor(Colors::Text), label.c_str());
        return max;
    }

    void InspectorPanel::DrawComponent(Scene::Component& component) {
        const Scene::ComponentTypeInfo* type = Scene::ComponentRegistry::Find(component.GetTypeID());
        if (!type) return;
        bool isTransform = !type->Create;
        bool open = m_Closed.count(type->ID) == 0;
        std::string label = Display::GetComponentLabel(*type);

        ImGui::PushID(static_cast<int>(type->ID));
        ImVec2 min = ImGui::GetCursorScreenPos();
        ImVec2 max = DrawBlockHeader(Display::GetComponentIcon(component), label, open);
        if (open) {
            m_Closed.erase(type->ID);
        } else {
            m_Closed.insert(type->ID);
        }

        // The block's menu, on the right.
        constexpr f32 kMenuButtonWidth = 26.0f;
        ImGui::SetCursorScreenPos(ImVec2(max.x - Sizes::Space2 - kMenuButtonWidth,
                                         min.y + std::floor((Sizes::TabHeight - Sizes::ControlHeight) * 0.5f)));
        if (Widgets::GhostButton(Icons::EllipsisVertical, ImVec2(kMenuButtonWidth, Sizes::ControlHeight))) {
            ImGui::OpenPopup("##ComponentMenu");
        }
        if (ImGui::BeginPopup("##ComponentMenu")) {
            if (ImGui::MenuItem(WithIcon(Icons::RefreshCw, "Reset").c_str())) m_ToReset = type->ID;
            // Every object keeps its transform.
            if (!isTransform) {
                ImGui::Separator();
                if (ImGui::MenuItem(WithIcon(Icons::Trash2, "Remove component").c_str())) m_ToRemove = type->ID;
            }
            ImGui::EndPopup();
        }
        ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));

        if (open) {
            ImGui::Dummy(ImVec2(0.0f, Sizes::Space3 - Sizes::Space2));
            ImGui::Indent(kBodyIndent);
            ImGui::PushItemWidth(-Sizes::Space4);
            bool changed = DrawProperties(*type->Properties, type->ToObject(&component));
            ImGui::PopItemWidth();
            ImGui::Unindent(kBodyIndent);
            ImGui::Dummy(ImVec2(0.0f, Sizes::Space4 - Sizes::Space2));
            if (changed) {
                component.ApplyPropertyChanges(m_Assets);
                m_Scene.MarkDirty();
            }
        }
        ImGui::PopID();
    }

    bool InspectorPanel::DrawProperties(const Scene::StructInfo& type, void* object) {
        bool changed = false;
        for (const Scene::PropertyInfo& property : type) {
            ImGui::PushID(property.Name);
            if (property.Type == Scene::PropertyType::Struct) {
                changed |= DrawProperties(*property.Struct, property.Get(object));
            } else {
                changed |= DrawProperty(property, property.Get(object));
            }
            ImGui::PopID();
        }
        return changed;
    }

    bool InspectorPanel::DrawProperty(const Scene::PropertyInfo& property, void* value) {
        using Scene::PropertyDisplay;
        using Scene::PropertyType;

        std::string label = Display::ToWords(property.Name);
        switch (property.Type) {
        case PropertyType::Bool:
            Fields::Label(label.c_str(), Sizes::LabelWidth);
            return Fields::Checkbox("##Value", *static_cast<bool*>(value));

        case PropertyType::F32: {
            Fields::Label(label.c_str(), Sizes::LabelWidth);
            f32& number = *static_cast<f32*>(value);
            if (property.Display == PropertyDisplay::Angle) {
                return Fields::Angle("##Value", number, property.Min, property.Max);
            }
            return Fields::Float("##Value", number, 0.01f, property.Min, property.Max, "%.3f");
        }

        case PropertyType::Vec3: {
            Vec3& vector = *static_cast<Vec3*>(value);
            if (property.Display == PropertyDisplay::Color) {
                Fields::Label(label.c_str(), Sizes::LabelWidth);
                return Fields::Color("##Value", vector);
            }
            Fields::Label(label.c_str(), kVectorLabelWidth);
            return Fields::Vec3Field("##Value", vector, 0.01f, "%.3f");
        }

        case PropertyType::Quat: return DrawRotation(label.c_str(), property, *static_cast<Quat*>(value));

        case PropertyType::Enum:
            Fields::Label(label.c_str(), Sizes::LabelWidth);
            return Fields::Enum("##Value", *property.Enum, value);

        case PropertyType::Mesh: {
            Fields::Label(label.c_str(), Sizes::LabelWidth);
            auto& mesh = *static_cast<Handle<Assets::MeshData>*>(value);
            const Assets::MeshData* data = m_Assets.GetMesh(mesh);
            Assets::AssetID dropped;
            auto accepts = [this](Assets::AssetID id) { return m_Assets.GetAnyType(id) == Assets::AssetType::Mesh; };
            switch (Fields::Asset("##Value", Icons::Box, data ? data->Name : std::string(), "Mesh", accepts, dropped)) {
            case Fields::AssetAction::Drop: mesh = m_Assets.LoadMesh(dropped); return true;
            case Fields::AssetAction::Clear: mesh = {}; return true;
            case Fields::AssetAction::None: break;
            }
            return false;
        }

        case PropertyType::Materials:
            return DrawMaterials(*static_cast<std::vector<Handle<Assets::MaterialData>>*>(value));

        case PropertyType::Struct: break;
        }
        return false;
    }

    bool InspectorPanel::DrawRotation(const char* label, const Scene::PropertyInfo& property, Quat& rotation) {
        // The angles are made again only when the rotation changed elsewhere, like in a gizmo.
        if (m_Euler.Object != m_Object || m_Euler.Property != &property || !IsClose(m_Euler.From, rotation)) {
            Vec3 degrees = glm::degrees(glm::eulerAngles(rotation));
            // Rounded, so float noise doesn't show as -0.0 or 89.99999.
            for (int i = 0; i < 3; ++i) {
                degrees[i] = std::round(degrees[i] * 1000.0f) / 1000.0f + 0.0f;
            }
            m_Euler = {m_Object, &property, rotation, degrees};
        }

        Fields::Label(label, kVectorLabelWidth);
        Vec3 degrees = m_Euler.Degrees;
        if (!Fields::Vec3Field("##Value", degrees, 0.5f, "%.1f\xc2\xb0")) return false;
        rotation = Quat(glm::radians(degrees));
        m_Euler.Degrees = degrees;
        m_Euler.From = rotation;
        return true;
    }

    bool InspectorPanel::DrawMaterials(std::vector<Handle<Assets::MaterialData>>& materials) {
        Fields::Label("Materials", Sizes::LabelWidth);
        std::string count = materials.empty() ? std::string("The mesh's own")
                                              : std::to_string(materials.size()) +
                                                    (materials.size() == 1 ? " slot" : " slots");
        RowText(count.c_str(), Colors::TextMuted);

        bool changed = false;
        ImGui::SameLine();
        if (Widgets::GhostButton(Icons::Plus, ImVec2(Sizes::ControlHeight, Sizes::ControlHeight))) {
            materials.emplace_back();
            changed = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add a slot, in place of the mesh's material of that index");
        if (!materials.empty()) {
            ImGui::SameLine(0.0f, Sizes::Space1);
            if (Widgets::GhostButton(Icons::Minus, ImVec2(Sizes::ControlHeight, Sizes::ControlHeight))) {
                materials.pop_back();
                changed = true;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove the last slot");
        }

        auto accepts = [this](Assets::AssetID id) { return m_Assets.GetAnyType(id) == Assets::AssetType::Material; };
        for (size_t i = 0; i < materials.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            // Slots sit under the list's label, a step in.
            std::string slot = "Slot " + std::to_string(i);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + Sizes::Space5);
            Fields::Label(slot.c_str(), Sizes::LabelWidth - Sizes::Space5);
            const Assets::MaterialData* data = m_Assets.GetMaterial(materials[i]);
            Assets::AssetID dropped;
            switch (Fields::Asset("##Value", Icons::Palette, data ? data->Name : std::string(), "Material", accepts,
                                  dropped)) {
            case Fields::AssetAction::Drop:
                materials[i] = m_Assets.LoadMaterial(dropped);
                changed = true;
                break;
            case Fields::AssetAction::Clear:
                materials[i] = {};
                changed = true;
                break;
            case Fields::AssetAction::None: break;
            }
            ImGui::PopID();
        }
        return changed;
    }

    void InspectorPanel::DrawAddComponent(Scene::GameObject& object) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 min = ImGui::GetCursorScreenPos();
        f32 width = ImGui::GetContentRegionAvail().x;
        draw->AddLine(min, ImVec2(min.x + width, min.y), ToImColor(Colors::Line));

        f32 buttonWidth = std::min(kAddButtonWidth, width - 2.0f * Sizes::Space4);
        ImGui::SetCursorScreenPos(ImVec2(min.x + std::floor((width - buttonWidth) * 0.5f), min.y + Sizes::Space5));
        ImGui::PushFont(GetFont(TextStyle::UIMedium));
        bool clicked = ImGui::Button(WithIcon(Icons::Plus, "Add component").c_str(),
                                     ImVec2(buttonWidth, Sizes::ControlHeight));
        ImGui::PopFont();
        ImVec2 below(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + Sizes::Space1);
        if (clicked) {
            m_AddFilter.clear();
            ImGui::OpenPopup("##AddComponent");
        }
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space5));

        // The types the object can still get, filtered by name.
        ImGui::SetNextWindowPos(below);
        ImGui::SetNextWindowSize(ImVec2(buttonWidth, 0.0f));
        if (ImGui::BeginPopup("##AddComponent")) {
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            Widgets::SearchField("##Filter", m_AddFilter, "Search components");
            ImGui::Separator();
            u32 shown = 0;
            for (Scene::ComponentTypeID id = 0; id < Scene::ComponentRegistry::GetCount(); ++id) {
                const Scene::ComponentTypeInfo* type = Scene::ComponentRegistry::Find(id);
                if (!type || !type->Create || object.GetComponent(id)) continue;
                std::string label = Display::GetComponentLabel(*type);
                if (!m_AddFilter.empty() && !ContainsIgnoringCase(label, m_AddFilter)) continue;
                ++shown;
                if (ImGui::MenuItem(WithIcon(Display::GetComponentIcon(*type->Defaults), label.c_str()).c_str())) {
                    object.AddComponent(id);
                    m_Closed.erase(id);
                    m_Scene.MarkDirty();
                }
            }
            if (shown == 0) Widgets::Text("Nothing to add", Colors::TextFaint);
            ImGui::EndPopup();
        }
    }

    void InspectorPanel::DrawAsset(Assets::AssetID asset) {
        std::optional<Assets::AssetType> type = m_Assets.GetAnyType(asset);
        if (!type) {
            ImGui::SetCursorPos(ImVec2(Sizes::Space4, ImGui::GetCursorPosY() + Sizes::Space7));
            Widgets::Text("The asset is gone.", Colors::TextMuted);
            return;
        }

        ImGui::PushID(asset.ToString().c_str());
        DrawAssetHeader(asset, *type);
        switch (*type) {
        case Assets::AssetType::Material: DrawMaterial(asset); break;
        case Assets::AssetType::Texture: DrawTexture(asset); break;
        case Assets::AssetType::Model: DrawModel(asset); break;
        case Assets::AssetType::Mesh: DrawMesh(asset); break;
        case Assets::AssetType::Scene:
            ImGui::SetCursorPosX(Sizes::Space4);
            Widgets::Text("Double-click the scene in Assets to open it.", Colors::TextMuted);
            break;
        }
        ImGui::PopID();
    }

    void InspectorPanel::DrawAssetHeader(Assets::AssetID asset, Assets::AssetType type) {
        static constexpr const char* kIcons[] = {Icons::Boxes, Icons::Image, Icons::Palette, Icons::Clapperboard,
                                                 Icons::Box};
        std::filesystem::path file = m_Assets.GetFile(asset);
        std::string name = file.empty() ? std::string() : file.stem().u8string();
        if (name.empty()) {
            if (const Assets::MaterialData* material = m_Assets.GetMaterial(m_Assets.LoadMaterial(asset));
                type == Assets::AssetType::Material && material) {
                name = material->Name;
            } else if (const Assets::MeshData* mesh = m_Assets.GetMesh(m_Assets.LoadMesh(asset));
                       type == Assets::AssetType::Mesh && mesh) {
                name = mesh->Name;
            }
        }

        bool isMaterialFile = type == Assets::AssetType::Material && !file.empty();
        bool writable = !file.empty() && m_Assets.IsWritable(asset);
        bool unsaved = m_Edits.IsUnsaved(asset);

        // The icon and the name, the unsaved mark, and the file under them.
        ImGui::SetCursorPos(ImVec2(Sizes::Space4, ImGui::GetCursorPosY() + Sizes::Space4));
        ImGui::AlignTextToFramePadding();
        Widgets::Text(kIcons[static_cast<size_t>(type)], Colors::TextMuted, TextStyle::Icons16);
        ImGui::SameLine(0.0f, Sizes::Space4);
        ImGui::AlignTextToFramePadding();
        Widgets::Text(name.c_str(), Colors::Text, TextStyle::UIStrong);
        if (unsaved) {
            ImGui::SameLine(0.0f, Sizes::Space3);
            ImGui::AlignTextToFramePadding();
            Widgets::Dot(Colors::Accent);
        }

        // Save and Revert for a material file, on the right; a badge for what can't be changed.
        f32 right = ImGui::GetWindowContentRegionMax().x - Sizes::Space4;
        if (isMaterialFile && writable) {
            ImGui::PushFont(GetFont(TextStyle::UIMedium));
            const ImGuiStyle& style = ImGui::GetStyle();
            f32 saveWidth = ImGui::CalcTextSize("Save").x + 2.0f * style.FramePadding.x + 14.0f + Sizes::Space3;
            f32 revertWidth = ImGui::CalcTextSize("Revert").x + 2.0f * style.FramePadding.x;
            ImGui::PopFont();
            ImGui::SameLine(std::max(right - saveWidth - revertWidth - Sizes::Space3, ImGui::GetCursorPosX()));
            if (Widgets::PrimaryButton(WithIcon(Icons::Save, "Save").c_str(), ImVec2(0.0f, 0.0f), unsaved)) {
                m_Edits.Save(asset);
            }
            ImGui::SameLine(0.0f, Sizes::Space3);
            ImGui::BeginDisabled(!unsaved);
            ImGui::PushFont(GetFont(TextStyle::UIMedium));
            if (ImGui::Button("Revert")) m_Edits.Revert(asset);
            ImGui::PopFont();
            ImGui::EndDisabled();
        } else if (!writable) {
            const char* badge = file.empty() ? "IN MODEL" : "READ-ONLY";
            ImFont* small = GetFont(TextStyle::UISmall);
            f32 badgeWidth = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, badge).x + 10.0f;
            ImGui::SameLine(std::max(right - badgeWidth, ImGui::GetCursorPosX()));
            ImGui::AlignTextToFramePadding();
            Widgets::Badge(badge, Colors::TextMuted);
        }

        // The file as its root names it, or the model the asset is inside.
        std::string where;
        if (!file.empty()) {
            const Assets::AssetRoot* root = m_Assets.FindRoot(file);
            where = (root ? root->Name + "/" : std::string()) +
                    (root ? file.lexically_relative(root->Folder).generic_u8string() : file.u8string());
        } else {
            where = "Inside a model, which can't be changed here";
        }
        ImGui::SetCursorPosX(Sizes::Space4 + 16.0f + Sizes::Space4);
        Widgets::Text(where.c_str(), Colors::TextMuted, file.empty() ? TextStyle::UISmall : TextStyle::MonoSmall);
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space3));
    }

    bool InspectorPanel::BeginAssetBlock(const char* icon, const char* label) {
        ImGui::PushID(label);
        bool open = m_ClosedAssetBlocks.count(label) == 0;
        ImVec2 min = ImGui::GetCursorScreenPos();
        ImVec2 max = DrawBlockHeader(icon, label, open);
        if (open) {
            m_ClosedAssetBlocks.erase(label);
        } else {
            m_ClosedAssetBlocks.insert(label);
        }
        ImGui::SetCursorScreenPos(ImVec2(min.x, max.y));
        if (!open) {
            ImGui::PopID();
            return false;
        }
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space3 - Sizes::Space2));
        ImGui::Indent(kBodyIndent);
        ImGui::PushItemWidth(-Sizes::Space4);
        return true;
    }

    void InspectorPanel::EndAssetBlock() {
        ImGui::PopItemWidth();
        ImGui::Unindent(kBodyIndent);
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space4 - Sizes::Space2));
        ImGui::PopID();
    }

    void InspectorPanel::DrawMaterial(Assets::AssetID asset) {
        struct Slot {
            const char* Label;
            Assets::MaterialTexture Texture;
        };
        static constexpr Slot kSlots[] = {
            {"Albedo", Assets::MaterialTexture::Albedo},       {"Normal", Assets::MaterialTexture::Normal},
            {"Metallic", Assets::MaterialTexture::Metallic},   {"Roughness", Assets::MaterialTexture::Roughness},
            {"Occlusion", Assets::MaterialTexture::Occlusion}, {"Emissive", Assets::MaterialTexture::Emissive},
        };

        const Assets::MaterialData* data = m_Assets.GetMaterial(m_Assets.LoadMaterial(asset));
        if (!data) {
            ImGui::SetCursorPosX(Sizes::Space4);
            Widgets::Text("The material can't be loaded.", Colors::Danger);
            return;
        }
        Assets::MaterialData values = *data;
        bool editable = !m_Assets.GetFile(asset).empty() && m_Assets.IsWritable(asset);
        bool changed = false;

        ImGui::BeginDisabled(!editable);
        if (BeginAssetBlock(Icons::SlidersHorizontal, "Surface")) {
            Vec3 color(values.BaseColor);
            Fields::Label("Base color", Sizes::LabelWidth);
            if (Fields::Color("##BaseColor", color)) {
                values.BaseColor = Vec4(color, values.BaseColor.a);
                changed = true;
            }
            Fields::Label("Opacity", Sizes::LabelWidth);
            changed |= Fields::Float("##Opacity", values.BaseColor.a, 0.005f, 0.0f, 1.0f, "%.3f");
            Fields::Label("Metallic", Sizes::LabelWidth);
            changed |= Fields::Float("##Metallic", values.Metallic, 0.005f, 0.0f, 1.0f, "%.3f");
            Fields::Label("Roughness", Sizes::LabelWidth);
            changed |= Fields::Float("##Roughness", values.Roughness, 0.005f, 0.0f, 1.0f, "%.3f");
            Fields::Label("Occlusion", Sizes::LabelWidth);
            changed |= Fields::Float("##Occlusion", values.Occlusion, 0.005f, 0.0f, 1.0f, "%.3f");
            Fields::Label("Emissive", Sizes::LabelWidth);
            changed |= Fields::Color("##Emissive", values.Emissive);
            EndAssetBlock();
        }

        if (BeginAssetBlock(Icons::Image, "Textures")) {
            ImFont* small = GetFont(TextStyle::UISmall);
            f32 badgeWidth = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, "LINEAR").x + 10.0f;
            constexpr f32 kThumbSize = 22.0f;
            auto accepts = [this](Assets::AssetID id) { return m_Assets.GetType(id) == Assets::AssetType::Texture; };
            for (const Slot& slot : kSlots) {
                ImGui::PushID(slot.Label);
                Handle<Assets::TextureData>& texture = values.Textures[static_cast<size_t>(slot.Texture)];
                Assets::AssetID textureID = m_Assets.GetID(texture);
                Fields::Label(slot.Label, Sizes::LabelWidth);

                // The image, the field and the color space the slot reads it in.
                ImVec2 thumbMin = ImGui::GetCursorScreenPos();
                ImGui::Dummy(ImVec2(kThumbSize, kThumbSize));
                ImDrawList* draw = ImGui::GetWindowDrawList();
                ImVec2 thumbMax(thumbMin.x + kThumbSize, thumbMin.y + kThumbSize);
                draw->AddRectFilled(thumbMin, thumbMax, ToImColor(Colors::BgField), 2.0f);
                if (textureID.IsValid()) {
                    draw->AddImageRounded(m_UI.GetTextureID(Renderer::OverlayTexture::FromAsset(texture)), thumbMin,
                                          thumbMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), IM_COL32_WHITE, 2.0f);
                }
                draw->AddRect(thumbMin, thumbMax, ToImColor(Colors::Line), 2.0f);
                ImGui::SameLine(0.0f, Sizes::Space3);

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - Sizes::Space4 - badgeWidth - Sizes::Space3);
                const Assets::TextureData* textureData = m_Assets.GetTexture(texture);
                std::string name = textureID.IsValid() && textureData ? textureData->Name : std::string();
                Assets::AssetID dropped;
                switch (Fields::Asset("##Texture", Icons::Image, name, "Texture", accepts, dropped)) {
                case Fields::AssetAction::Drop:
                    texture = m_Assets.LoadTexture(dropped, Assets::GetSlotColorSpace(slot.Texture));
                    changed = true;
                    break;
                case Fields::AssetAction::Clear:
                    texture = {};
                    changed = true;
                    break;
                case Fields::AssetAction::None: break;
                }

                ImGui::SameLine(0.0f, Sizes::Space3);
                Assets::ColorSpace space = textureData && textureID.IsValid() ? textureData->ColorSpace
                                                                              : Assets::GetSlotColorSpace(slot.Texture);
                // Centered on the field's height.
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                                     std::floor((Sizes::ControlHeight - ImGui::GetTextLineHeight()) * 0.5f));
                Widgets::Badge(space == Assets::ColorSpace::SRGB ? "SRGB" : "LINEAR", Colors::TextMuted);
                ImGui::PopID();
            }
            ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x - Sizes::Space4);
            Widgets::Text("Color space follows the slot. Override it on the texture.", Colors::TextFaint,
                          TextStyle::UISmall);
            ImGui::PopTextWrapPos();
            EndAssetBlock();
        }
        ImGui::EndDisabled();

        if (changed && editable) m_Edits.Change(asset, std::move(values));
    }

    u64 InspectorPanel::GetTextureImage(Assets::AssetID texture) {
        Handle<Assets::TextureData> handle = m_Assets.FindLoadedTexture(texture);
        if (!handle.IsValid()) {
            Assets::ColorSpace space = m_Assets.GetColorSpace(texture) == Assets::TextureColorSpace::Linear
                                           ? Assets::ColorSpace::Linear
                                           : Assets::ColorSpace::SRGB;
            handle = m_Assets.LoadTexture(texture, space);
        }
        return handle.IsValid() ? m_UI.GetTextureID(Renderer::OverlayTexture::FromAsset(handle)) : 0;
    }

    void InspectorPanel::DrawTexture(Assets::AssetID asset) {
        static const std::vector<std::string> kSpaces = {"From use", "sRGB", "Linear"};
        u64 image = GetTextureImage(asset);
        const Assets::TextureData* data = m_Assets.GetTexture(m_Assets.FindLoadedTexture(asset));

        if (BeginAssetBlock(Icons::Image, "Image")) {
            // The image as large as fits, keeping its shape.
            f32 width = ImGui::GetContentRegionAvail().x - Sizes::Space4;
            f32 aspect =
                data && data->Height > 0 ? static_cast<f32>(data->Width) / static_cast<f32>(data->Height) : 1.0f;
            ImVec2 size(width, width / aspect);
            if (size.y > 256.0f) size = ImVec2(256.0f * aspect, 256.0f);
            ImVec2 min = ImGui::GetCursorScreenPos();
            ImGui::Dummy(size);
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(min, ImVec2(min.x + size.x, min.y + size.y), ToImColor(Colors::BgField));
            if (image) draw->AddImage(image, min, ImVec2(min.x + size.x, min.y + size.y));
            draw->AddRect(min, ImVec2(min.x + size.x, min.y + size.y), ToImColor(Colors::Line));
            ImGui::Dummy(ImVec2(0.0f, Sizes::Space2));

            if (data) {
                char text[64];
                std::snprintf(text, sizeof(text), "%u \xc3\x97 %u", data->Width, data->Height);
                Fields::Label("Size", Sizes::LabelWidth);
                ImGui::AlignTextToFramePadding();
                Widgets::Text(text, Colors::Text, TextStyle::Mono);
                Fields::Label("Pixels", Sizes::LabelWidth);
                ImGui::AlignTextToFramePadding();
                Widgets::Text(data->PixelType == Assets::PixelType::Float32 ? "32-bit float (HDR)" : "8-bit",
                              Colors::Text);
            }

            // Kept in the .vkasset: changing it loads the texture again where materials use it.
            Fields::Label("Color space", Sizes::LabelWidth);
            int current = static_cast<int>(m_Assets.GetColorSpace(asset));
            ImGui::BeginDisabled(!m_Assets.IsWritable(asset));
            if (Fields::Choice("##ColorSpace", kSpaces, current)) {
                m_Assets.SetColorSpace(asset, static_cast<Assets::TextureColorSpace>(current));
            }
            ImGui::EndDisabled();
            ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x - Sizes::Space4);
            Widgets::Text("From use: sRGB in the albedo and emissive slots, linear in the others.", Colors::TextFaint,
                          TextStyle::UISmall);
            ImGui::PopTextWrapPos();
            EndAssetBlock();
        }
    }

    void InspectorPanel::DrawModel(Assets::AssetID asset) {
        std::vector<Assets::SubAsset> subAssets = m_Assets.GetSubAssets(asset);
        if (BeginAssetBlock(Icons::Layers, "Contents")) {
            ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x - Sizes::Space4);
            if (subAssets.empty()) {
                Widgets::Text("Open the model's tile in Assets to list its meshes and materials.", Colors::TextMuted);
            }
            ImGui::PopTextWrapPos();
            for (const Assets::SubAsset& subAsset : subAssets) {
                ImGui::AlignTextToFramePadding();
                Widgets::Text(subAsset.Type == Assets::AssetType::Mesh ? Icons::Box : Icons::Palette,
                              Colors::TextMuted);
                ImGui::SameLine(0.0f, Sizes::Space3);
                Widgets::Text(subAsset.Name.c_str(), Colors::Text);
            }
            ImGui::Dummy(ImVec2(0.0f, Sizes::Space2));
            ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x - Sizes::Space4);
            Widgets::Text("Drag the model into the Hierarchy or the Viewport to place it.", Colors::TextFaint,
                          TextStyle::UISmall);
            ImGui::PopTextWrapPos();
            EndAssetBlock();
        }
    }

    void InspectorPanel::DrawMesh(Assets::AssetID asset) {
        const Assets::MeshData* mesh = m_Assets.GetMesh(m_Assets.LoadMesh(asset));
        if (!mesh || !BeginAssetBlock(Icons::Box, "Geometry")) return;
        auto row = [](const char* label, size_t value) {
            Fields::Label(label, Sizes::LabelWidth);
            ImGui::AlignTextToFramePadding();
            Widgets::Text(std::to_string(value).c_str(), Colors::Text, TextStyle::Mono);
        };
        row("Vertices", mesh->Vertices.size());
        row("Triangles", mesh->Indices.size() / 3);
        row("Submeshes", mesh->Submeshes.size());
        EndAssetBlock();
    }

} // namespace VKE::Editor
