#include "Widgets.h"

#include <ui/Icons.h>

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>

namespace VKE::Editor::Widgets {

    using UI::GetFont;
    using UI::TextStyle;
    namespace Colors = UI::Colors;
    namespace Sizes = UI::Sizes;

    ImVec4 ToVec4(u32 rgba) {
        return ImVec4(static_cast<f32>((rgba >> 24) & 0xFF) / 255.0f, static_cast<f32>((rgba >> 16) & 0xFF) / 255.0f,
                      static_cast<f32>((rgba >> 8) & 0xFF) / 255.0f, static_cast<f32>(rgba & 0xFF) / 255.0f);
    }

    namespace {
        // Fill, hover, pressed, text and border of a button.
        bool StyledButton(const char* label, const ImVec2& size, u32 fill, u32 hover, u32 active, u32 text, u32 border) {
            ImGui::PushStyleColor(ImGuiCol_Button, ToVec4(fill));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToVec4(hover));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToVec4(active));
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(text));
            ImGui::PushStyleColor(ImGuiCol_Border, ToVec4(border));
            ImGui::PushFont(GetFont(TextStyle::UIMedium));
            bool clicked = ImGui::Button(label, size);
            ImGui::PopFont();
            ImGui::PopStyleColor(5);
            return clicked;
        }
    } // namespace

    bool PrimaryButton(const char* label, const ImVec2& size, bool enabled) {
        ImGui::BeginDisabled(!enabled);
        bool clicked = StyledButton(label, size, Colors::Accent, Colors::AccentHover, Colors::AccentHover,
                                    Colors::OnAccent, Colors::Accent);
        ImGui::EndDisabled();
        return clicked;
    }

    bool GhostButton(const char* label, const ImVec2& size) {
        return StyledButton(label, size, 0, Colors::BgHover, Colors::BgActive, Colors::Text, 0);
    }

    bool ToggleButton(const char* label, bool on) {
        ImGui::PushFont(GetFont(TextStyle::UISmall));
        // 20 px tall, like the design's filter chips.
        f32 padding = std::max(0.0f, (20.0f - ImGui::GetFontSize()) * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Sizes::Space3, padding));
        ImGui::PushStyleColor(ImGuiCol_Button, ToVec4(on ? Colors::BgRaised : 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToVec4(Colors::BgHover));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToVec4(Colors::BgActive));
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(on ? Colors::Text : Colors::TextMuted));
        ImGui::PushStyleColor(ImGuiCol_Border, ToVec4(0));
        bool clicked = ImGui::Button(label);
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar();
        ImGui::PopFont();
        return clicked;
    }

    void Badge(const char* text, u32 color) {
        ImFont* font = GetFont(TextStyle::UISmall);
        ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text);
        ImVec2 size(textSize.x + 10.0f, 16.0f);

        // Centered on the line it sits in.
        ImVec2 position = ImGui::GetCursorScreenPos();
        position.y += std::floor((ImGui::GetTextLineHeight() - size.y) * 0.5f);
        ImGui::ItemSize(ImVec2(size.x, ImGui::GetTextLineHeight()));

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 end(position.x + size.x, position.y + size.y);
        draw->AddRect(position, end, UI::ToImColor(color), 2.0f);
        draw->AddText(font, font->FontSize,
                      ImVec2(position.x + 5.0f, position.y + std::floor((size.y - textSize.y) * 0.5f)),
                      UI::ToImColor(color), text);
    }

    bool SearchField(const char* id, std::string& text, const char* hint, f32 width) {
        std::string fullHint = std::string(UI::Icons::Search) + "  " + hint;
        ImGui::SetNextItemWidth(width > 0.0f ? width : -FLT_MIN);
        return ImGui::InputTextWithHint(id, fullHint.c_str(), &text);
    }

    void Text(const char* text, u32 color, TextStyle style) {
        ImGui::PushFont(GetFont(style));
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(color));
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    void PushDialogStyle() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Sizes::RadiusLg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, Sizes::Border);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Sizes::Space7, 20.0f));
        // Modal windows take the popup colors.
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ToVec4(Colors::BgPanel));
        ImGui::PushStyleColor(ImGuiCol_Border, ToVec4(Colors::ControlBorder));
    }

    void PopDialogStyle() {
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }

    bool DialogHeader(const char* title) {
        ImGui::PushFont(GetFont(TextStyle::Heading));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(title);
        ImGui::PopFont();

        f32 buttonSize = Sizes::ControlHeight;
        ImGui::SameLine(ImGui::GetContentRegionMax().x - buttonSize);
        bool clicked = GhostButton(UI::Icons::X, ImVec2(buttonSize, buttonSize));
        ImGui::Dummy(ImVec2(0.0f, Sizes::Space4));
        return clicked;
    }

    void Dot(u32 color) {
        constexpr f32 kRadius = 3.5f;
        ImVec2 position = ImGui::GetCursorScreenPos();
        f32 height = ImGui::GetTextLineHeight();
        ImGui::ItemSize(ImVec2(kRadius * 2.0f, height));
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(position.x + kRadius, position.y + height * 0.5f), kRadius,
                                                    UI::ToImColor(color));
    }

} // namespace VKE::Editor::Widgets
