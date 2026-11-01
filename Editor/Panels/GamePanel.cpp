#include "GamePanel.h"
#include "EditorScene.h"
#include "Widgets.h"

#include <renderer/Renderer.h>
#include <scene/GameObject.h>
#include <scene/Scene.h>
#include <scene/components/CameraComponent.h>
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

    GamePanel::GamePanel(EditorScene& scene, Renderer::Renderer& renderer, UI::UISystem& ui)
        : m_Scene(scene), m_Renderer(renderer), m_UI(ui) {
    }

    void GamePanel::Create() {
        m_Texture = m_Renderer.CreateViewTexture(640, 360);
    }

    void GamePanel::Destroy() {
        m_Renderer.DestroyViewTexture(m_Texture);
        m_Texture = {};
    }

    const char* GamePanel::GetWindowName() {
        static const std::string name = std::string(Icons::Gamepad2) + "  Game###Game";
        return name.c_str();
    }

    void GamePanel::Draw(bool* open, const std::string& stats) {
        m_Visible = false;
        m_Focused = false;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        bool shown = ImGui::Begin(GetWindowName(), open);
        ImGui::PopStyleVar();
        if (!shown) {
            ImGui::End();
            return;
        }

        Scene::CameraComponent* camera = m_Scene.GetScene().GetMainCamera();
        if (!camera) {
            const char* note = "The scene has no camera.";
            ImVec2 size = ImGui::CalcTextSize(note);
            ImVec2 available = ImGui::GetContentRegionAvail();
            ImGui::SetCursorPos(ImVec2(std::floor((available.x - size.x) * 0.5f), std::floor(available.y * 0.5f)));
            Widgets::Text(note, Colors::TextMuted);
            ImGui::End();
            return;
        }

        m_Focused = ImGui::IsWindowFocused();
        ImVec2 size = ImGui::GetContentRegionAvail();
        size.x = std::max(size.x, 1.0f);
        size.y = std::max(size.y, 1.0f);
        m_Renderer.ResizeViewTexture(m_Texture, static_cast<u32>(size.x), static_cast<u32>(size.y));
        ImVec2 min = ImGui::GetCursorScreenPos();
        ImVec2 max(min.x + std::floor(size.x), min.y + std::floor(size.y));
        m_ViewX = min.x - ImGui::GetMainViewport()->Pos.x;
        m_ViewY = min.y - ImGui::GetMainViewport()->Pos.y;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddImage(m_UI.GetTextureID(Renderer::OverlayTexture::FromView(m_Texture)), min, max);
        m_Visible = true;

        // The camera's name at the top and the frame's numbers at the bottom, in pills.
        ImFont* mono = UI::GetFont(UI::TextStyle::MonoSmall);
        auto pill = [&](const std::string& text, f32 y) {
            ImVec2 textSize = mono->CalcTextSizeA(mono->FontSize, FLT_MAX, 0.0f, text.c_str());
            ImVec2 pillMin(min.x + Sizes::Space4, y);
            ImVec2 pillMax(pillMin.x + textSize.x + 2.0f * Sizes::Space4, y + 24.0f);
            draw->AddRectFilled(pillMin, pillMax, UI::ToImColor(Colors::Overlay), Sizes::RadiusMd);
            draw->AddText(mono, mono->FontSize,
                          ImVec2(pillMin.x + Sizes::Space4, std::floor(y + (24.0f - textSize.y) * 0.5f)),
                          UI::ToImColor(Colors::OverlayText), text.c_str());
        };
        pill(camera->GetOwner().GetName(), min.y + Sizes::Space4);
        pill(stats, max.y - Sizes::Space4 - 24.0f);
        ImGui::End();
    }

} // namespace VKE::Editor
