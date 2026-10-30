#pragma once

#include <core/Types.h>

#include <vector>

namespace VKE::Renderer {

    // One light's wish for shadow space: `FaceCount` square tiles of `Size` texels.
    struct ShadowRequest {
        // Whatever the caller needs to find its light again.
        u32 Light = 0;
        u32 FaceCount = 1;
        // A power of two. Pack lowers it if the atlas is short of room, and leaves 0 if the
        // light got no space at all.
        u32 Size = 0;
        // The most important lights keep their size the longest.
        f32 Priority = 0.0f;
        // Set by Pack: the light's first tile; the others follow it.
        u32 FirstTile = 0;
    };

    // A square of the atlas, in texels.
    struct ShadowTile {
        u32 X = 0;
        u32 Y = 0;
        u32 Size = 0;
    };

    // Smallest tile a shadow gets.
    constexpr u32 kMinShadowTileSize = 128;

    // Divides a square atlas of `atlasSize` texels, a power of two, among `requests`. If they
    // don't fit, the least important lights lose resolution first, and only when every light is
    // at the smallest size do they lose their shadows. Orders `requests` by priority.
    void PackShadowAtlas(std::vector<ShadowRequest>& requests, u32 atlasSize, std::vector<ShadowTile>& outTiles);

} // namespace VKE::Renderer
