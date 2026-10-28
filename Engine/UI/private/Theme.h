#pragma once

#include <core/Types.h>

struct ImGuiStyle;

namespace VKE::UI {

    // Fills `style` with the design system's dark theme, following the token map in its
    // "Mapa para o ImGui" section. `uiFontSize` is the UI font's size in the atlas, which
    // the frame padding is fitted to so controls come out 22 px tall.
    void ApplyTheme(ImGuiStyle& style, f32 uiFontSize);

} // namespace VKE::UI
