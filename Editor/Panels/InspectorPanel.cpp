#include "InspectorPanel.h"
#include "Display.h"
#include "EditorScene.h"
#include "Fields.h"
#include "Widgets.h"

#include <assets/AssetManager.h>
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

    InspectorPanel::InspectorPanel(EditorScene& scene, Assets::AssetManager& assets)
        : m_Scene(scene), m_Assets(assets) {
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

        Scene::GameObject* object = m_Scene.GetSelected();
        if (!object) {
            const char* note = "Select an object to see its components.";
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

    void InspectorPanel::DrawComponent(Scene::Component& component) {
        const Scene::ComponentTypeInfo* type = Scene::ComponentRegistry::Find(component.GetTypeID());
        if (!type) return;
        bool isTransform = !type->Create;
        bool open = m_Closed.count(type->ID) == 0;
        std::string label = Display::GetComponentLabel(*type);

        ImGui::PushID(static_cast<int>(type->ID));
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 min = ImGui::GetCursorScreenPos();
        f32 width = ImGui::GetContentRegionAvail().x;
        ImVec2 max(min.x + width, min.y + Sizes::TabHeight);

        // The header: a click opens or closes the block.
        ImGui::SetNextItemAllowOverlap();
        if (ImGui::InvisibleButton("##Header", ImVec2(width, Sizes::TabHeight))) {
            if (open) {
                m_Closed.insert(type->ID);
            } else {
                m_Closed.erase(type->ID);
            }
            open = !open;
        }
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
        draw->AddText(font, font->FontSize, ImVec2(x, textY), ToImColor(Colors::TextMuted),
                      Display::GetComponentIcon(component));
        x += 14.0f + Sizes::Space3;
        f32 nameY = min.y + std::floor((Sizes::TabHeight - strong->FontSize) * 0.5f);
        draw->AddText(strong, strong->FontSize, ImVec2(x, nameY), ToImColor(Colors::Text), label.c_str());

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
            auto accepts = [this](Assets::AssetID id) { return m_Assets.LoadMesh(id).IsValid(); };
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

        auto accepts = [this](Assets::AssetID id) { return m_Assets.LoadMaterial(id).IsValid(); };
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

} // namespace VKE::Editor
