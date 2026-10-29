#pragma once

#include <core/Types.h>

// The VKE design system as the UI uses it: its colors and text styles. The ImGui theme is
// built from these (private/Theme.cpp); editor code draws its own shapes with them.

struct ImFont;

namespace VKE::UI {

    // Colors of the design system's dark theme (tokens.json), sRGB-encoded, as 0xRRGGBBAA.
    // The token each one comes from is its name in kebab case.
    namespace Colors {
        constexpr u32 BgFrame = 0x131211FF;
        constexpr u32 BgPanel = 0x1B1A18FF;
        constexpr u32 BgHeader = 0x232120FF;
        constexpr u32 BgField = 0x0F0E0DFF;
        constexpr u32 BgFieldHover = 0x191816FF;
        constexpr u32 BgRaised = 0x2A2825FF;
        constexpr u32 BgHover = 0x33302CFF;
        constexpr u32 BgActive = 0x3D3A35FF;
        constexpr u32 Line = 0x2B2926FF;
        constexpr u32 ControlBorder = 0x6D685FFF;
        constexpr u32 Text = 0xEBE7E0FF;
        constexpr u32 TextMuted = 0xA8A196FF;
        constexpr u32 TextFaint = 0x6E6961FF;
        constexpr u32 Accent = 0xEE9A3AFF;
        constexpr u32 AccentHover = 0xF5AD57FF;
        constexpr u32 OnAccent = 0x1C1307FF;
        constexpr u32 AccentSoft = 0xEE9A3A33;
        constexpr u32 SelectionUnfocused = 0x3A3732FF;
        constexpr u32 Focus = Accent;
        constexpr u32 Info = 0x62A8E8FF;
        constexpr u32 Live = 0x52C48DFF;
        constexpr u32 LiveSoft = 0x52C48D26;
        constexpr u32 Warning = 0xE6CF5AFF;
        constexpr u32 Danger = 0xF0675CFF;
        constexpr u32 DangerSoft = 0xF0675C26;
        constexpr u32 AxisX = 0xE8564BFF;
        constexpr u32 AxisY = 0x84C34FFF;
        constexpr u32 AxisZ = 0x4F8FF0FF;
        constexpr u32 Overlay = 0x131211D9;
        constexpr u32 OverlayText = 0xEBE7E0FF;
        constexpr u32 Scrim = 0x000000A6;
    } // namespace Colors

    // Sizes from the design system's spacing, radius and size tokens, in pixels at 100% scale.
    namespace Sizes {
        constexpr f32 Space1 = 2.0f;
        constexpr f32 Space2 = 4.0f;
        constexpr f32 Space3 = 6.0f;
        constexpr f32 Space4 = 8.0f;
        constexpr f32 Space5 = 12.0f;
        constexpr f32 Space6 = 16.0f;
        constexpr f32 Space7 = 24.0f;
        constexpr f32 Space8 = 32.0f;
        constexpr f32 RadiusSm = 3.0f;
        constexpr f32 RadiusMd = 5.0f;
        constexpr f32 RadiusLg = 8.0f;
        constexpr f32 ControlHeight = 22.0f;
        constexpr f32 RowHeight = 22.0f;
        constexpr f32 ConsoleRowHeight = 20.0f;
        constexpr f32 TabHeight = 26.0f;
        constexpr f32 MenuBarHeight = 24.0f;
        constexpr f32 ToolbarHeight = 34.0f;
        constexpr f32 StatusBarHeight = 22.0f;
        constexpr f32 LabelWidth = 112.0f;
        constexpr f32 Indent = 14.0f;
        constexpr f32 IconLarge = 32.0f;
        constexpr f32 Border = 1.0f;
    } // namespace Sizes

    // An 0xRRGGBBAA color in ImGui's packed order (ImU32).
    constexpr u32 ToImColor(u32 rgba) {
        return ((rgba >> 24) & 0xFF) | (((rgba >> 16) & 0xFF) << 8) | (((rgba >> 8) & 0xFF) << 16) |
               ((rgba & 0xFF) << 24);
    }

    // The design system's text styles; each is one font in the atlas. The UI styles carry
    // the icons (Icons.h) at 14 px, UISmall at 12 px.
    enum class TextStyle {
        UI,
        UIMedium,
        UIStrong,
        UISmall,
        Heading,
        Title,
        Display,
        Mono,
        MonoSmall,
        // Only icons: 16 px for toolbars, 32 px for asset tiles.
        Icons16,
        Icons32,
        Count,
    };

    // The font for `style`, once a UISystem has built the atlas; null before that.
    ImFont* GetFont(TextStyle style);

} // namespace VKE::UI
