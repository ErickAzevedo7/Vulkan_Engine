#pragma once

#include <core/Types.h>
#include <ui/Style.h>

#include <imgui.h>

#include <string>

// Controls of the design system that ImGui has no style for, drawn with ImGui's own widgets
// and the design system's colors (ui/Style.h).

namespace VKE::Editor::Widgets {

    // An 0xRRGGBBAA color from ui/Style.h, as ImGui's float color.
    ImVec4 ToVec4(u32 rgba);

    // The amber button for a screen's main action.
    bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f), bool enabled = true);
    // A button with no fill until the mouse is over it.
    bool GhostButton(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f));
    // A small button that stays lit while `on`, like the Console's level filters.
    bool ToggleButton(const char* label, bool on);

    // A small label in capitals inside a 1 px border, like MISSING or READ-ONLY.
    void Badge(const char* text, u32 color);

    // A text field with a magnifier in its hint. `width` 0 fills the line.
    bool SearchField(const char* id, std::string& text, const char* hint, f32 width = 0.0f);

    // Text in one of the design system's colors and styles.
    void Text(const char* text, u32 color, UI::TextStyle style = UI::TextStyle::UI);

    // Modal dialogs: rounded, bordered and padded as the design system's dialog. Push before
    // ImGui::BeginPopupModal and pop right after it, whether it opened or not.
    void PushDialogStyle();
    void PopDialogStyle();

    // A dialog's title on the left and a close button on the right; true when that is clicked.
    bool DialogHeader(const char* title);

    // A filled circle the size of the text, like the unsaved mark after a scene's name.
    void Dot(u32 color);

} // namespace VKE::Editor::Widgets
