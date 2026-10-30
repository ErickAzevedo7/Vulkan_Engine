#include "ViewportPanel.h"
#include "EditorScene.h"
#include "Fields.h"
#include "Gizmo/GizmoDrawing.h"
#include "Widgets.h"

#include <assets/AssetManager.h>

#include <renderer/Renderer.h>
#include <scene/GameObject.h>
#include <scene/Scene.h>
#include <scene/components/CameraComponent.h>
#include <scene/components/TransformComponent.h>
#include <ui/Icons.h>
#include <ui/Style.h>
#include <ui/UISystem.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace VKE::Editor {

    namespace Colors = UI::Colors;
    namespace Icons = UI::Icons;
    namespace Sizes = UI::Sizes;

    namespace {
        // A press that moves less than this before its release is a click, not a drag.
        constexpr f32 kClickPixels = 4.0f;

        Vec2 ToVec2(const ImVec2& v) {
            return {v.x, v.y};
        }

        // An icon button of the Viewport's toolbar; `on` marks the active choice.
        bool ToolButton(const char* icon, bool on, const char* tooltip) {
            ImGui::PushFont(UI::GetFont(UI::TextStyle::Icons16));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 4.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, Widgets::ToVec4(on ? Colors::BgActive : 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Widgets::ToVec4(Colors::BgHover));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Widgets::ToVec4(Colors::BgActive));
            ImGui::PushStyleColor(ImGuiCol_Text, Widgets::ToVec4(on ? Colors::Accent : Colors::TextMuted));
            ImGui::PushStyleColor(ImGuiCol_Border, Widgets::ToVec4(0));
            bool clicked = ImGui::Button(icon);
            ImGui::PopStyleColor(5);
            ImGui::PopStyleVar();
            ImGui::PopFont();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", tooltip);
            return clicked;
        }

        MarkerIcon GetGlyph(ImFont* font, const char* icon) {
            MarkerIcon result;
            // The icons are single characters of up to four UTF-8 bytes.
            const auto* bytes = reinterpret_cast<const unsigned char*>(icon);
            u32 codepoint = bytes[0];
            if (bytes[0] >= 0xF0) {
                codepoint = ((bytes[0] & 0x07u) << 18) | ((bytes[1] & 0x3Fu) << 12) | ((bytes[2] & 0x3Fu) << 6) |
                            (bytes[3] & 0x3Fu);
            } else if (bytes[0] >= 0xE0) {
                codepoint = ((bytes[0] & 0x0Fu) << 12) | ((bytes[1] & 0x3Fu) << 6) | (bytes[2] & 0x3Fu);
            } else if (bytes[0] >= 0xC0) {
                codepoint = ((bytes[0] & 0x1Fu) << 6) | (bytes[1] & 0x3Fu);
            }
            const ImFontGlyph* glyph = font ? font->FindGlyphNoFallback(static_cast<ImWchar>(codepoint)) : nullptr;
            if (!glyph) return result;
            result.UVMin = Vec2(glyph->U0, glyph->V0);
            result.UVMax = Vec2(glyph->U1, glyph->V1);
            f32 width = glyph->X1 - glyph->X0;
            f32 height = glyph->Y1 - glyph->Y0;
            f32 larger = std::max(std::max(width, height), 1.0f);
            result.Shape = Vec2(width, height) / larger;
            return result;
        }
    } // namespace

    ViewportPanel::ViewportPanel(EditorScene& scene, Assets::AssetManager& assets, Renderer::Renderer& renderer,
                                 UI::UISystem& ui)
        : m_Scene(scene), m_Renderer(renderer), m_UI(ui), m_Markers(assets) {
    }

    void ViewportPanel::Create() {
        m_Texture = m_Renderer.CreateViewTexture(640, 360);
    }

    void ViewportPanel::Destroy() {
        m_Renderer.DestroyViewTexture(m_Texture);
        m_Texture = {};
    }

    const char* ViewportPanel::GetWindowName() {
        static const std::string name = std::string(Icons::Box) + "  Viewport###Viewport";
        return name.c_str();
    }

    void ViewportPanel::LoadIcons() {
        ImFont* font = UI::GetFont(UI::TextStyle::Icons32);
        if (!font) return;
        MarkerIcons icons;
        icons.Texture = m_UI.GetFontTexture();
        icons.Directional = GetGlyph(font, Icons::Sun);
        icons.Point = GetGlyph(font, Icons::Lightbulb);
        icons.Spot = GetGlyph(font, Icons::Flashlight);
        icons.Camera = GetGlyph(font, Icons::Video);
        m_Markers.SetIcons(icons);
        m_IconsLoaded = true;
    }

    void ViewportPanel::PlaceCamera() {
        const Scene::Scene& scene = m_Scene.GetScene();
        m_PlacedFor = &scene;
        if (const Scene::CameraComponent* camera = scene.GetMainCamera()) {
            const Transform& world = camera->GetOwner().GetTransform().GetWorld();
            m_Camera.LookAt(world.Position, world.Position + world.GetForward() * 8.0f);
        } else {
            m_Camera.LookAt(Vec3(6.0f, 4.0f, 8.0f), Vec3(0.0f));
        }
    }

    void ViewportPanel::ReceivePick() {
        Renderer::MeshInstanceID mesh;
        if (!m_Renderer.TakePickResult(m_Texture, mesh)) return;
        const Scene::Scene& scene = m_Scene.GetScene();
        if (m_PickedIn == &scene) m_Scene.Select(scene.GetMeshOwner(mesh));
    }

    void ViewportPanel::Draw(bool* open, const std::string& stats) {
        m_Visible = false;
        m_World.Clear();
        ReceivePick();
        if (m_PlacedFor != &m_Scene.GetScene()) PlaceCamera();
        if (!m_IconsLoaded) LoadIcons();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        bool shown = ImGui::Begin(GetWindowName(), open);
        ImGui::PopStyleVar();
        if (!shown) {
            m_Navigation = Navigation::None;
            ImGui::End();
            return;
        }

        ImVec2 size = ImGui::GetContentRegionAvail();
        size.x = std::max(size.x, 1.0f);
        size.y = std::max(size.y, 1.0f);
        m_Renderer.ResizeViewTexture(m_Texture, static_cast<u32>(size.x), static_cast<u32>(size.y));
        ImVec2 min = ImGui::GetCursorScreenPos();
        ImVec2 max(min.x + std::floor(size.x), min.y + std::floor(size.y));
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddImage(m_UI.GetTextureID(Renderer::OverlayTexture::FromView(m_Texture)), min, max);

        // Takes the mouse over the scene; the toolbar drawn later still gets it first.
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##Scene", size,
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                                   ImGuiButtonFlags_MouseButtonMiddle);
        bool hovered = ImGui::IsItemHovered();
        m_Visible = true;
        Assets::AssetID droppedModel;
        if (ImGui::BeginDragDropTarget()) {
            ImGuiDragDropFlags flags = ImGuiDragDropFlags_AcceptBeforeDelivery;
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(Fields::kAssetPayload, flags)) {
                Assets::AssetID asset = *static_cast<const Assets::AssetID*>(payload->Data);
                if (payload->IsDelivery() && m_Scene.GetAssets().GetType(asset) == Assets::AssetType::Model) {
                    droppedModel = asset;
                }
            }
            ImGui::EndDragDropTarget();
        }

        HandleCamera(hovered, size.y);
        HandleShortcuts(hovered);

        ScreenView view = ScreenView::From(m_Camera.GetView(), ToVec2(size));
        Vec2 mouse = ToVec2(ImGui::GetIO().MousePos) - ToVec2(min);
        bool gizmoBusy = HandleGizmo(view, mouse, hovered);
        HandleClick(view, mouse, hovered, gizmoBusy);

        // A dropped model goes where the mouse's ray meets the ground, or a few meters ahead.
        if (droppedModel.IsValid()) {
            Ray ray = view.GetRay(mouse);
            f32 distance = 0.0f;
            constexpr f32 kAhead = 5.0f;
            if (!IntersectPlane(ray, Vec3(0.0f), Vec3(0.0f, 1.0f, 0.0f), distance) || distance > 200.0f) {
                distance = kAhead;
            }
            Vec3 position = ray.Origin + ray.Direction * distance;
            if (Scene::GameObject* model = m_Scene.InstantiateModel(droppedModel, nullptr, position)) {
                m_Scene.Select(model);
            }
        }

        Scene::GameObject* selected = m_Scene.GetSelected();
        m_Markers.Draw(m_Scene.GetScene(), selected, view, m_World);
        if (selected) {
            const Scene::TransformComponent& transform = selected->GetTransform();
            GizmoPose pose{transform.GetWorld().Position, transform.GetWorld().Rotation, transform.GetLocal().Scale};
            DrawTransformGizmo(m_Gizmo, view, pose, m_World);
        }

        DrawToolbar(ToVec2(min));

        // The frame's numbers in a pill over the scene, bottom left; the fly speed while flying.
        ImFont* mono = UI::GetFont(UI::TextStyle::MonoSmall);
        auto pill = [&](const std::string& text, f32 x, bool alignRight) {
            ImVec2 textSize = mono->CalcTextSizeA(mono->FontSize, FLT_MAX, 0.0f, text.c_str());
            f32 width = textSize.x + 2.0f * Sizes::Space4;
            ImVec2 pillMin(alignRight ? x - width : x, max.y - Sizes::Space4 - 24.0f);
            ImVec2 pillMax(pillMin.x + width, pillMin.y + 24.0f);
            draw->AddRectFilled(pillMin, pillMax, UI::ToImColor(Colors::Overlay), Sizes::RadiusMd);
            draw->AddText(mono, mono->FontSize,
                          ImVec2(pillMin.x + Sizes::Space4, std::floor(pillMin.y + (24.0f - textSize.y) * 0.5f)),
                          UI::ToImColor(Colors::OverlayText), text.c_str());
        };
        pill(stats, min.x + Sizes::Space4, false);
        if (m_Navigation == Navigation::Fly) {
            char speed[48];
            std::snprintf(speed, sizeof(speed), "Fly speed %.1f m/s", m_Camera.GetFlySpeed());
            pill(speed, max.x - Sizes::Space4, true);
        }

        ImGui::End();
    }

    void ViewportPanel::HandleCamera(bool hovered, f32 viewportHeight) {
        ImGuiIO& io = ImGui::GetIO();
        Vec2 delta = ToVec2(io.MouseDelta);

        // A navigation starts with a press over the scene and lasts until its button is released.
        if (m_Navigation == Navigation::None && hovered) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !m_Gizmo.IsDragging()) {
                m_Navigation = Navigation::Fly;
            } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
                m_Navigation = Navigation::Pan;
            } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.KeyAlt) {
                m_Navigation = Navigation::Orbit;
            }
        }

        switch (m_Navigation) {
            case Navigation::Fly: {
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    m_Navigation = Navigation::None;
                    break;
                }
                auto axis = [](ImGuiKey positive, ImGuiKey negative) {
                    return (ImGui::IsKeyDown(positive) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(negative) ? 1.0f : 0.0f);
                };
                Vec3 move(axis(ImGuiKey_D, ImGuiKey_A), axis(ImGuiKey_E, ImGuiKey_Q), axis(ImGuiKey_W, ImGuiKey_S));
                m_Camera.Fly(delta, move, io.DeltaTime, io.KeyShift);
                if (io.MouseWheel != 0.0f) m_Camera.ChangeFlySpeed(io.MouseWheel);
                return;
            }
            case Navigation::Orbit:
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    m_Navigation = Navigation::None;
                    break;
                }
                m_Camera.Orbit(delta);
                break;
            case Navigation::Pan:
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
                    m_Navigation = Navigation::None;
                    break;
                }
                m_Camera.Pan(delta, viewportHeight);
                break;
            case Navigation::None: break;
        }

        if (hovered && io.MouseWheel != 0.0f) m_Camera.Zoom(io.MouseWheel);
    }

    void ViewportPanel::HandleShortcuts(bool hovered) {
        if (m_Navigation == Navigation::Fly || ImGui::GetIO().WantTextInput) return;
        if (!hovered && !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
        if (m_Gizmo.IsDragging()) return;

        if (ImGui::IsKeyPressed(ImGuiKey_W, false)) m_Gizmo.Mode = GizmoMode::Move;
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) m_Gizmo.Mode = GizmoMode::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) m_Gizmo.Mode = GizmoMode::Scale;
        if (ImGui::IsKeyPressed(ImGuiKey_F, false)) FocusSelected();
    }

    void ViewportPanel::FocusSelected() {
        Scene::GameObject* selected = m_Scene.GetSelected();
        if (!selected) return;
        AABB bounds;
        if (m_Markers.GetBounds(*selected, bounds)) {
            m_Camera.Focus((bounds.Min + bounds.Max) * 0.5f, glm::length(bounds.Max - bounds.Min) * 0.5f);
        } else {
            m_Camera.Focus(selected->GetTransform().GetWorld().Position, 1.0f);
        }
    }

    bool ViewportPanel::HandleGizmo(const ScreenView& view, const Vec2& mouse, bool hovered) {
        Scene::GameObject* selected = m_Scene.GetSelected();
        if (!selected) return false;

        ImGuiIO& io = ImGui::GetIO();
        GizmoPointer pointer;
        pointer.Position = mouse;
        pointer.Over = hovered && m_Navigation == Navigation::None && !io.KeyAlt;
        pointer.Pressed = pointer.Over && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        pointer.Down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        // Ctrl does the opposite of the toolbar's choice.
        pointer.Snap = m_Snap != io.KeyCtrl;
        pointer.Cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsMouseClicked(ImGuiMouseButton_Right);

        Scene::TransformComponent& transform = selected->GetTransform();
        GizmoPose pose{transform.GetWorld().Position, transform.GetWorld().Rotation, transform.GetLocal().Scale};
        if (m_Gizmo.Update(view, pointer, pose)) {
            // Only what the mode changes, so the other values don't drift through conversions.
            switch (m_Gizmo.Mode) {
                case GizmoMode::Move: transform.SetWorldPosition(pose.Position); break;
                case GizmoMode::Rotate: transform.SetWorldRotation(pose.Rotation); break;
                case GizmoMode::Scale: transform.SetLocalScale(pose.Scale); break;
            }
            m_Scene.MarkDirty();
        }
        return m_Gizmo.IsDragging() || m_Gizmo.GetHovered().Part != GizmoPart::None;
    }

    void ViewportPanel::HandleClick(const ScreenView& view, const Vec2& mouse, bool hovered, bool gizmoBusy) {
        if (hovered && !gizmoBusy && m_Navigation == Navigation::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_ClickPending = true;
            m_ClickStart = mouse;
        }
        if (!m_ClickPending || !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) return;
        m_ClickPending = false;
        if (glm::length(mouse - m_ClickStart) > kClickPixels) return;

        // Icons are over the scene, so they are picked first; then the meshes, on the GPU.
        Scene::Scene& scene = m_Scene.GetScene();
        if (Scene::GameObject* object = m_Markers.PickIcon(scene, view, mouse)) {
            m_Scene.Select(object);
            return;
        }
        if (mouse.x < 0.0f || mouse.y < 0.0f) return;
        m_Renderer.RequestPick(m_Texture, static_cast<u32>(mouse.x), static_cast<u32>(mouse.y));
        m_PickedIn = &scene;
    }

    void ViewportPanel::DrawToolbar(const Vec2& min) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        // The buttons go on top, and the pill behind them is drawn once their size is known.
        draw->ChannelsSplit(2);
        draw->ChannelsSetCurrent(1);

        ImGui::SetCursorScreenPos(ImVec2(min.x + Sizes::Space4 + Sizes::Space2, min.y + Sizes::Space4 + Sizes::Space2));
        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Sizes::Space1, 0.0f));
        if (ToolButton(Icons::Move3D, m_Gizmo.Mode == GizmoMode::Move, "Move (W)")) m_Gizmo.Mode = GizmoMode::Move;
        ImGui::SameLine();
        if (ToolButton(Icons::Rotate3D, m_Gizmo.Mode == GizmoMode::Rotate, "Rotate (E)")) {
            m_Gizmo.Mode = GizmoMode::Rotate;
        }
        ImGui::SameLine();
        if (ToolButton(Icons::Scaling, m_Gizmo.Mode == GizmoMode::Scale, "Scale (R)")) m_Gizmo.Mode = GizmoMode::Scale;

        ImGui::SameLine(0.0f, Sizes::Space4);
        bool local = m_Gizmo.Space == GizmoSpace::Local;
        if (ToolButton(local ? Icons::Box : Icons::Globe, false,
                       local ? "Local axes: the object's own. Click for the world's"
                             : "World axes. Click for the object's own")) {
            m_Gizmo.Space = local ? GizmoSpace::World : GizmoSpace::Local;
        }

        ImGui::SameLine(0.0f, Sizes::Space4);
        if (ToolButton(Icons::Magnet, m_Snap, "Snap to steps (hold Ctrl to switch)")) m_Snap = !m_Snap;
        ImGui::SameLine();
        if (ToolButton(Icons::ChevronDown, false, "Snap steps")) ImGui::OpenPopup("SnapSteps");

        ImGui::SameLine(0.0f, Sizes::Space4);
        if (ToolButton(Icons::Grid3x3, m_Markers.ShowGrid, "Grid")) m_Markers.ShowGrid = !m_Markers.ShowGrid;
        ImGui::SameLine();
        if (ToolButton(Icons::Lightbulb, m_Markers.ShowIcons, "Light and camera icons")) {
            m_Markers.ShowIcons = !m_Markers.ShowIcons;
        }
        ImGui::PopStyleVar();
        ImGui::EndGroup();

        ImVec2 groupMin = ImGui::GetItemRectMin();
        ImVec2 groupMax = ImGui::GetItemRectMax();
        draw->ChannelsSetCurrent(0);
        draw->AddRectFilled(ImVec2(groupMin.x - Sizes::Space2, groupMin.y - Sizes::Space2),
                            ImVec2(groupMax.x + Sizes::Space2, groupMax.y + Sizes::Space2),
                            UI::ToImColor(Colors::Overlay), Sizes::RadiusMd);
        draw->ChannelsMerge();

        if (ImGui::BeginPopup("SnapSteps")) {
            ImGui::TextUnformatted("Snap steps");
            ImGui::Separator();
            f32 degrees = glm::degrees(m_Gizmo.RotateStep);
            ImGui::SetNextItemWidth(120.0f);
            ImGui::DragFloat("Move", &m_Gizmo.MoveStep, 0.01f, 0.001f, 1000.0f, "%.3f m");
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::DragFloat("Rotate", &degrees, 0.5f, 0.1f, 180.0f, "%.1f\xc2\xb0")) {
                m_Gizmo.RotateStep = glm::radians(degrees);
            }
            ImGui::SetNextItemWidth(120.0f);
            ImGui::DragFloat("Scale", &m_Gizmo.ScaleStep, 0.01f, 0.001f, 100.0f, "%.3f");
            ImGui::EndPopup();
        }
    }

} // namespace VKE::Editor
