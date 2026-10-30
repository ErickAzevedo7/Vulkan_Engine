#include "ShadowAtlas.h"

#include <algorithm>

namespace VKE::Renderer {

    namespace {
        // Sizes are counted in cells of the smallest tile.
        u64 GetArea(const ShadowRequest& request) {
            u64 side = request.Size / kMinShadowTileSize;
            return side * side * request.FaceCount;
        }

        // The even bits of `value`, packed together.
        u32 TakeEvenBits(u32 value) {
            value &= 0x55555555u;
            value = (value | (value >> 1)) & 0x33333333u;
            value = (value | (value >> 2)) & 0x0F0F0F0Fu;
            value = (value | (value >> 4)) & 0x00FF00FFu;
            value = (value | (value >> 8)) & 0x0000FFFFu;
            return value;
        }
    } // namespace

    void PackShadowAtlas(std::vector<ShadowRequest>& requests, u32 atlasSize, std::vector<ShadowTile>& outTiles) {
        outTiles.clear();
        std::stable_sort(requests.begin(), requests.end(), [](const ShadowRequest& a, const ShadowRequest& b) {
            return a.Priority > b.Priority;
        });

        u64 capacity = static_cast<u64>(atlasSize / kMinShadowTileSize) * (atlasSize / kMinShadowTileSize);
        u64 used = 0;
        for (const ShadowRequest& request : requests) {
            used += GetArea(request);
        }

        // Halves the sizes from the least important light up, again and again, until all fit.
        while (used > capacity) {
            bool shrunk = false;
            for (auto it = requests.rbegin(); it != requests.rend() && used > capacity; ++it) {
                if (it->Size <= kMinShadowTileSize) continue;
                used -= GetArea(*it);
                it->Size /= 2;
                used += GetArea(*it);
                shrunk = true;
            }
            if (shrunk) continue;

            // All at the smallest size: the least important light goes without.
            for (auto it = requests.rbegin(); it != requests.rend() && used > capacity; ++it) {
                used -= GetArea(*it);
                it->Size = 0;
            }
        }

        struct Face {
            u32 Tile;
            u32 Size;
        };
        std::vector<Face> faces;
        for (ShadowRequest& request : requests) {
            if (request.Size == 0) continue;
            request.FirstTile = static_cast<u32>(outTiles.size());
            for (u32 face = 0; face < request.FaceCount; ++face) {
                faces.push_back({static_cast<u32>(outTiles.size()), request.Size});
                outTiles.push_back({0, 0, request.Size});
            }
        }

        // Largest first, along a curve that fills squares inside squares: a tile always starts
        // on a multiple of its size, so tiles of powers of two leave no gaps.
        std::stable_sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) { return a.Size > b.Size; });
        u32 cursor = 0;
        for (const Face& face : faces) {
            ShadowTile& tile = outTiles[face.Tile];
            tile.X = TakeEvenBits(cursor) * kMinShadowTileSize;
            tile.Y = TakeEvenBits(cursor >> 1) * kMinShadowTileSize;
            u32 side = face.Size / kMinShadowTileSize;
            cursor += side * side;
        }
    }

} // namespace VKE::Renderer
