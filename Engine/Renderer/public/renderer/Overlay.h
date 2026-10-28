#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/Types.h>
#include <math/MathTypes.h>
#include <vector>

// The 2D layer the renderer draws over the window at the end of each frame: triangles
// with a texture and a clip rectangle per command. Interfaces build these lists; the
// layer knows nothing about what they draw.

namespace VKE::Renderer {

    // A texture the renderer owns for the 2D layer, made from pixels (a font atlas, icons).
    struct OverlayImage;
    using OverlayImageID = Handle<OverlayImage>;

    // A texture of the size the caller asks for, that DrawScene can draw a scene into and
    // the 2D layer can show (an editor viewport).
    struct ViewTexture;
    using ViewTextureID = Handle<ViewTexture>;

    // What a 2D command samples. Empty samples white, for plain colored shapes.
    struct OverlayTexture {
        enum class Kind : u8 {
            None,
            Image,
            View,
            // A texture asset, as loaded for rendering.
            Asset,
        };

        Kind Type = Kind::None;
        u32 Index = 0;
        u32 Generation = 0;

        static OverlayTexture FromImage(OverlayImageID id) { return {Kind::Image, id.Index, id.Generation}; }
        static OverlayTexture FromView(ViewTextureID id) { return {Kind::View, id.Index, id.Generation}; }
        static OverlayTexture FromAsset(Handle<Assets::TextureData> id) { return {Kind::Asset, id.Index, id.Generation}; }

        bool operator==(const OverlayTexture& other) const {
            return Type == other.Type && Index == other.Index && Generation == other.Generation;
        }
        bool operator!=(const OverlayTexture& other) const { return !(*this == other); }
    };

    struct OverlayVertex {
        // In window pixels, origin at the top left.
        Vec2 Position;
        Vec2 UV;
        // R, G, B, A bytes in that order in memory, sRGB-encoded like CSS colors.
        u32 Color;
    };
    static_assert(sizeof(OverlayVertex) == 20, "OverlayVertex must match the overlay pipeline's vertex input");

    struct OverlayCommand {
        u32 FirstIndex = 0;
        u32 IndexCount = 0;
        // Added to every index of the command.
        i32 VertexOffset = 0;
        // Pixels outside (min x, min y, max x, max y) are not drawn.
        Vec4 ClipRect{0.0f};
        OverlayTexture Texture;
    };

    // Drawn in command order, each command over the previous ones, alpha blended.
    struct OverlayDrawList {
        std::vector<OverlayVertex> Vertices;
        std::vector<u16> Indices;
        std::vector<OverlayCommand> Commands;

        void Clear() {
            Vertices.clear();
            Indices.clear();
            Commands.clear();
        }
    };

} // namespace VKE::Renderer
