#pragma once

#include <core/Types.h>

#include <filesystem>
#include <vector>

namespace VKE::UI {

    // A TrueType file in memory, with the vertical metrics that relate its em to ImGui's
    // font size: ImGui sizes a font by the height from its ascender to its descender, while
    // the design system, like CSS, sizes it by the em.
    struct FontFile {
        std::vector<Byte> Data;
        f32 UnitsPerEm = 0.0f;
        // From the hhea table, in font units; the descender is negative.
        f32 Ascender = 0.0f;
        f32 Descender = 0.0f;

        // Logs an error and returns an invalid file when it cannot be read or parsed.
        static FontFile Load(const std::filesystem::path& file);

        bool IsValid() const { return !Data.empty() && UnitsPerEm > 0.0f; }

        // The size to give ImGui for text whose em is `em` pixels.
        f32 PixelHeightForEm(f32 em) const { return em * (Ascender - Descender) / UnitsPerEm; }
        // Distance from the top of the line to the baseline at that size.
        f32 AscentForEm(f32 em) const { return em * Ascender / UnitsPerEm; }
    };

} // namespace VKE::UI
