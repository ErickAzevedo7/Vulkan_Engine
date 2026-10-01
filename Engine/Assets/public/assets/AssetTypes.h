#pragma once

#include <core/Handle.h>
#include <core/Types.h>
#include <math/MathTypes.h>
#include <array>
#include <string>
#include <vector>

namespace VKE::Assets {

    struct Vertex {
        Vec3 Position;
        Vec3 Normal;
        // Origin at the top-left of the texture.
        Vec2 UV;
        // xyz = tangent, w = bitangent sign.
        Vec4 Tangent;
    };

    struct TextureData;
    struct MaterialData;

    struct Submesh {
        u32 FirstIndex = 0;
        u32 IndexCount = 0;
        // Index into MeshData::Materials.
        u32 MaterialIndex = 0;
    };

    // Geometry from one file. All submeshes share the vertex and index arrays.
    struct MeshData {
        std::string Name;
        std::vector<Vertex> Vertices;
        std::vector<u32> Indices;
        std::vector<Submesh> Submeshes;
        std::vector<Handle<MaterialData>> Materials;
    };

    enum class ColorSpace {
        Linear,
        SRGB,
    };

    enum class PixelType {
        UNorm8,
        // Used by .hdr images, whose values go above 1.
        Float32,
    };

    // Four channels per pixel, top row first.
    struct TextureData {
        std::string Name;
        u32 Width = 0;
        u32 Height = 0;
        ColorSpace ColorSpace = ColorSpace::Linear;
        PixelType PixelType = PixelType::UNorm8;
        // Raw bytes, whose meaning follows PixelType.
        std::vector<u8> Pixels;
    };

    enum class MaterialTexture : u32 {
        Albedo,
        Normal,
        Metallic,
        Roughness,
        Occlusion,
        Emissive,
        Count,
    };

    // Each texture is scaled by its matching factor.
    struct MaterialData {
        std::string Name;
        Vec4 BaseColor{1.0f};
        Vec3 Emissive{0.0f};
        f32 Metallic = 1.0f;
        f32 Roughness = 1.0f;
        f32 Occlusion = 1.0f;
        std::array<Handle<TextureData>, static_cast<size_t>(MaterialTexture::Count)> Textures{};
    };

} // namespace VKE::Assets
