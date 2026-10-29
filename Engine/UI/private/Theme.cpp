#include "Theme.h"

#include <ui/Style.h>

#include <imgui.h>

#include <algorithm>

namespace VKE::UI {

    namespace {
        ImVec4 Color(u32 rgba) {
            return ImVec4(static_cast<f32>((rgba >> 24) & 0xFF) / 255.0f, static_cast<f32>((rgba >> 16) & 0xFF) / 255.0f,
                          static_cast<f32>((rgba >> 8) & 0xFF) / 255.0f, static_cast<f32>(rgba & 0xFF) / 255.0f);
        }

        ImVec4 WithAlpha(u32 rgba, f32 alpha) {
            ImVec4 color = Color(rgba);
            color.w = alpha;
            return color;
        }

        constexpr f32 kControlHeight = Sizes::ControlHeight;
        constexpr f32 kSpace1 = Sizes::Space1;
        constexpr f32 kSpace2 = Sizes::Space2;
        constexpr f32 kSpace3 = Sizes::Space3;
        constexpr f32 kSpace4 = Sizes::Space4;
        constexpr f32 kRadiusSm = Sizes::RadiusSm;
        constexpr f32 kRadiusMd = Sizes::RadiusMd;
        constexpr f32 kIndent = Sizes::Indent;
        constexpr f32 kBorder = Sizes::Border;
    } // namespace

    void ApplyTheme(ImGuiStyle& style, f32 uiFontSize) {
        style.WindowPadding = ImVec2(kSpace4, kSpace4);
        style.FramePadding = ImVec2(kSpace3, std::max(0.0f, (kControlHeight - uiFontSize) * 0.5f));
        style.ItemSpacing = ImVec2(kSpace4, kSpace2);
        style.ItemInnerSpacing = ImVec2(kSpace3, kSpace2);
        style.CellPadding = ImVec2(kSpace3, kSpace1);
        style.IndentSpacing = kIndent;
        style.ScrollbarSize = 10.0f;
        style.GrabMinSize = 8.0f;

        // Docked panels are told apart by the gap between them, controls by their border.
        style.WindowBorderSize = 0.0f;
        style.ChildBorderSize = 0.0f;
        style.FrameBorderSize = kBorder;
        style.PopupBorderSize = kBorder;
        style.TabBorderSize = 0.0f;
        style.TabBarBorderSize = 0.0f;
        style.TabBarOverlineSize = 2.0f;
        style.DockingSeparatorSize = 2.0f;
        style.SeparatorTextBorderSize = kBorder;

        style.WindowRounding = 0.0f;
        style.ChildRounding = 0.0f;
        style.FrameRounding = kRadiusSm;
        style.TabRounding = kRadiusSm;
        style.GrabRounding = kRadiusSm;
        style.ScrollbarRounding = kRadiusSm;
        style.PopupRounding = kRadiusMd;

        style.WindowMenuButtonPosition = ImGuiDir_None;

        ImVec4* colors = style.Colors;
        const ImVec4 transparent(0.0f, 0.0f, 0.0f, 0.0f);

        colors[ImGuiCol_Text] = Color(Colors::Text);
        colors[ImGuiCol_TextDisabled] = Color(Colors::TextFaint);
        colors[ImGuiCol_WindowBg] = Color(Colors::BgPanel);
        colors[ImGuiCol_ChildBg] = Color(Colors::BgPanel);
        colors[ImGuiCol_PopupBg] = Color(Colors::BgRaised);
        colors[ImGuiCol_Border] = Color(Colors::ControlBorder);
        colors[ImGuiCol_BorderShadow] = transparent;
        colors[ImGuiCol_FrameBg] = Color(Colors::BgField);
        colors[ImGuiCol_FrameBgHovered] = Color(Colors::BgFieldHover);
        colors[ImGuiCol_FrameBgActive] = Color(Colors::BgField);
        colors[ImGuiCol_TitleBg] = Color(Colors::BgFrame);
        colors[ImGuiCol_TitleBgActive] = Color(Colors::BgFrame);
        colors[ImGuiCol_TitleBgCollapsed] = Color(Colors::BgFrame);
        colors[ImGuiCol_MenuBarBg] = Color(Colors::BgFrame);
        colors[ImGuiCol_ScrollbarBg] = transparent;
        colors[ImGuiCol_ScrollbarGrab] = Color(Colors::BgActive);
        colors[ImGuiCol_ScrollbarGrabHovered] = Color(Colors::ControlBorder);
        colors[ImGuiCol_ScrollbarGrabActive] = Color(Colors::TextMuted);
        // ImGui's own checkbox: an amber check on the field. The design's filled box needs
        // the editor's checkbox, drawn by hand.
        colors[ImGuiCol_CheckMark] = Color(Colors::Accent);
        colors[ImGuiCol_SliderGrab] = Color(Colors::Accent);
        colors[ImGuiCol_SliderGrabActive] = Color(Colors::AccentHover);
        colors[ImGuiCol_Button] = Color(Colors::BgRaised);
        colors[ImGuiCol_ButtonHovered] = Color(Colors::BgHover);
        colors[ImGuiCol_ButtonActive] = Color(Colors::BgActive);
        colors[ImGuiCol_Header] = Color(Colors::AccentSoft);
        colors[ImGuiCol_HeaderHovered] = Color(Colors::BgHover);
        colors[ImGuiCol_HeaderActive] = Color(Colors::BgActive);
        colors[ImGuiCol_Separator] = Color(Colors::Line);
        colors[ImGuiCol_SeparatorHovered] = Color(Colors::ControlBorder);
        colors[ImGuiCol_SeparatorActive] = Color(Colors::Accent);
        colors[ImGuiCol_ResizeGrip] = transparent;
        colors[ImGuiCol_ResizeGripHovered] = Color(Colors::ControlBorder);
        colors[ImGuiCol_ResizeGripActive] = Color(Colors::Accent);
        colors[ImGuiCol_Tab] = Color(Colors::BgFrame);
        colors[ImGuiCol_TabHovered] = Color(Colors::BgHover);
        colors[ImGuiCol_TabSelected] = Color(Colors::BgPanel);
        colors[ImGuiCol_TabSelectedOverline] = Color(Colors::Accent);
        colors[ImGuiCol_TabDimmed] = Color(Colors::BgFrame);
        colors[ImGuiCol_TabDimmedSelected] = Color(Colors::BgPanel);
        colors[ImGuiCol_TabDimmedSelectedOverline] = transparent;
        colors[ImGuiCol_DockingPreview] = Color(Colors::AccentSoft);
        colors[ImGuiCol_DockingEmptyBg] = Color(Colors::BgFrame);
        colors[ImGuiCol_PlotLines] = Color(Colors::TextMuted);
        colors[ImGuiCol_PlotLinesHovered] = Color(Colors::Text);
        colors[ImGuiCol_PlotHistogram] = Color(Colors::Accent);
        colors[ImGuiCol_PlotHistogramHovered] = Color(Colors::AccentHover);
        colors[ImGuiCol_TableHeaderBg] = Color(Colors::BgHeader);
        colors[ImGuiCol_TableBorderStrong] = Color(Colors::Line);
        colors[ImGuiCol_TableBorderLight] = Color(Colors::Line);
        colors[ImGuiCol_TableRowBg] = transparent;
        colors[ImGuiCol_TableRowBgAlt] = WithAlpha(Colors::BgHeader, 0.4f);
        colors[ImGuiCol_TextLink] = Color(Colors::Info);
        colors[ImGuiCol_TextSelectedBg] = Color(Colors::AccentSoft);
        colors[ImGuiCol_DragDropTarget] = Color(Colors::Accent);
        colors[ImGuiCol_NavCursor] = Color(Colors::Focus);
        colors[ImGuiCol_NavWindowingHighlight] = Color(Colors::Accent);
        colors[ImGuiCol_NavWindowingDimBg] = Color(Colors::Scrim);
        colors[ImGuiCol_ModalWindowDimBg] = Color(Colors::Scrim);
    }

} // namespace VKE::UI
