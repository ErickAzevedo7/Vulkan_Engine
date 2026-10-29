#include "ViewportPanel.h"

#include <renderer/Renderer.h>
#include <ui/Icons.h>
#include <ui/Style.h>
#include <ui/UISystem.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace VKE::Editor {

    namespace Colors = UI::Colors;
    namespace Sizes = UI::Sizes;

    ViewportPanel::ViewportPanel(Renderer::Renderer& renderer, UI::UISystem& ui) : m_Renderer(renderer), m_UI(ui) {
    }

    void ViewportPanel::Create() {
        m_Texture = m_Renderer.CreateViewTexture(640, 360);
    }

    void ViewportPanel::Destroy() {
        m_Renderer.DestroyViewTexture(m_Texture);
        m_Texture = {};
    }

    const char* ViewportPanel::GetWindowName() {
        static const std::string name = std::string(UI::Icons::Box) + "  Viewport###Viewport";
        return name.c_str();
    }

    void ViewportPanel::Draw(bool* open, const std::string& stats, bool hasCamera) {
        m_Visible = false;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        bool shown = ImGui::Begin(GetWindowName(), open);
        ImGui::PopStyleVar();
        if (!shown) {
            ImGui::End();
            return;
        }

        ImVec2 size = ImGui::GetContentRegionAvail();
        u32 width = static_cast<u32>(std::max(size.x, 1.0f));
        u32 height = static_cast<u32>(std::max(size.y, 1.0f));
        m_Renderer.ResizeViewTexture(m_Texture, width, height);
        ImVec2 min = ImGui::GetCursorScreenPos();
        ImTextureID texture = m_UI.GetTextureID(Renderer::OverlayTexture::FromView(m_Texture));
        ImGui::Image(texture, ImVec2(static_cast<f32>(width), static_cast<f32>(height)));
        m_Visible = true;

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImFont* font = UI::GetFont(UI::TextStyle::UI);
        if (!hasCamera) {
            const char* note = "No camera in the scene";
            ImVec2 noteSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, note);
            draw->AddText(font, font->FontSize,
                          ImVec2(std::floor(min.x + (size.x - noteSize.x) * 0.5f),
                                 std::floor(min.y + (size.y - noteSize.y) * 0.5f)),
                          UI::ToImColor(Colors::TextMuted), note);
        }

        // The frame's numbers in a pill over the scene, bottom left.
        ImFont* mono = UI::GetFont(UI::TextStyle::MonoSmall);
        ImVec2 textSize = mono->CalcTextSizeA(mono->FontSize, FLT_MAX, 0.0f, stats.c_str());
        ImVec2 pillMin(min.x + Sizes::Space4, min.y + size.y - Sizes::Space4 - 24.0f);
        ImVec2 pillMax(pillMin.x + textSize.x + 2.0f * Sizes::Space4, pillMin.y + 24.0f);
        draw->AddRectFilled(pillMin, pillMax, UI::ToImColor(Colors::Overlay), Sizes::RadiusMd);
        draw->AddText(mono, mono->FontSize,
                      ImVec2(pillMin.x + Sizes::Space4, std::floor(pillMin.y + (24.0f - textSize.y) * 0.5f)),
                      UI::ToImColor(Colors::OverlayText), stats.c_str());

        ImGui::End();
    }

} // namespace VKE::Editor
