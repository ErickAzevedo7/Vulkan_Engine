#pragma once

#include <core/Types.h>
#include <math/MathTypes.h>
#include <renderer/Overlay.h>
#include <vector>

// Shapes in world coordinates drawn over a scene in a view texture, after its color is mapped
// to displayable range, so they keep their colors: lines with a width in pixels and triangles
// with an optional texture. Tools build these lists (grids, icons, handles); the renderer
// knows nothing about what they draw.

namespace VKE::Renderer {

    enum class WorldPrimitive : u8 {
        // Segments widened to quads on the GPU, so their width stays the same on screen.
        Lines,
        Triangles,
    };

    enum class WorldDepth : u8 {
        // Hidden behind the scene's surfaces.
        Tested,
        // Drawn over everything, in list order.
        OnTop,
    };

    // How the shapes added next are drawn.
    struct WorldStyle {
        WorldDepth Depth = WorldDepth::Tested;
        // Triangles only. Empty draws plain color.
        OverlayTexture Texture;
        // Shapes fade out between these distances from the camera. 0 for FadeEnd never fades.
        f32 FadeStart = 0.0f;
        f32 FadeEnd = 0.0f;

        bool operator==(const WorldStyle& other) const {
            return Depth == other.Depth && Texture == other.Texture && FadeStart == other.FadeStart &&
                   FadeEnd == other.FadeEnd;
        }
        bool operator!=(const WorldStyle& other) const { return !(*this == other); }
    };

    struct WorldVertex {
        Vec3 Position;
        // Lines: the segment's other end. Unused by triangles.
        Vec3 Other;
        // Triangles: texture coordinates. Lines: x is the side of the segment (-1 or 1), y is
        // 0 at the segment's start and 1 at its end.
        Vec2 UV;
        // R, G, B, A bytes in that order in memory, sRGB-encoded like CSS colors.
        u32 Color;
        // Lines: in pixels. Unused by triangles.
        f32 Width;
    };
    static_assert(sizeof(WorldVertex) == 40, "WorldVertex must match the world pipeline's vertex input");

    struct WorldCommand {
        u32 FirstIndex = 0;
        u32 IndexCount = 0;
        WorldPrimitive Primitive = WorldPrimitive::Triangles;
        WorldStyle Style;
    };

    // Drawn in command order, alpha blended. The Add functions extend the last command while
    // the primitive and style stay the same.
    struct WorldDrawList {
        std::vector<WorldVertex> Vertices;
        std::vector<u32> Indices;
        std::vector<WorldCommand> Commands;

        void Clear();

        void SetStyle(const WorldStyle& style) { m_Style = style; }
        const WorldStyle& GetStyle() const { return m_Style; }

        void AddLine(const Vec3& from, const Vec3& to, u32 color, f32 width);
        void AddTriangle(const Vec3& a, const Vec3& b, const Vec3& c, u32 color);
        // Corners in order around the quad; UVs from uvMin at the first to uvMax at the third.
        void AddQuad(const Vec3 corners[4], u32 color, const Vec2& uvMin = Vec2(0.0f), const Vec2& uvMax = Vec2(1.0f));

    private:
        // The command the next shapes go into.
        WorldCommand& GetCommand(WorldPrimitive primitive);

        WorldStyle m_Style;
    };

} // namespace VKE::Renderer
