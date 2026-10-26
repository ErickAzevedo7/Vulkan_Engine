#pragma once

#include <core/Handle.h>
#include <core/IDUtils.h>
#include <core/Types.h>
#include <math/MathTypes.h>
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace VKE::Assets {

    // Identifies an asset across runs, and across renames and moves of its file, which keeps it
    // in a .vkasset file next to itself. Meshes and materials inside a model file have their own,
    // kept in the model's .vkasset. A default-constructed ID is invalid.
    struct AssetID {
        u64 Value = 0;

        bool IsValid() const { return Value != 0; }
        bool operator==(const AssetID& other) const { return Value == other.Value; }
        bool operator!=(const AssetID& other) const { return Value != other.Value; }

        // A new random ID, never zero.
        static AssetID Generate() { return {GenerateID()}; }
        // As IDs are written in files (see FormatID). Parsing returns an invalid ID for anything
        // FormatID wouldn't write.
        std::string ToString() const { return FormatID(Value); }
        static AssetID Parse(const std::string& text) { return {ParseID(text)}; }
    };

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
        // The asset it was loaded from, set by the AssetManager. Invalid for assets made in code.
        AssetID ID;
        std::vector<Vertex> Vertices;
        std::vector<u32> Indices;
        std::vector<Submesh> Submeshes;
        std::vector<Handle<MaterialData>> Materials;
    };

    // One node of a model file's hierarchy.
    struct ModelNode {
        std::string Name;
        // Relative to the parent node.
        Mat4 LocalTransform{1.0f};
        // Index into ModelData::Nodes, or -1 for the root.
        i32 Parent = -1;
        // Invalid when the node has no geometry.
        Handle<MeshData> Mesh;
    };

    // The node hierarchy of a model file. Parents always come before their children,
    // and the first node is the root.
    struct ModelData {
        std::string Name;
        // The asset it was loaded from, set by the AssetManager. Invalid for assets made in code.
        AssetID ID;
        std::vector<ModelNode> Nodes;
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
        // The asset it was loaded from, set by the AssetManager. Invalid for assets made in code.
        AssetID ID;
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
        // The asset it was loaded from, set by the AssetManager. Invalid for assets made in code.
        AssetID ID;
        Vec4 BaseColor{1.0f};
        Vec3 Emissive{0.0f};
        f32 Metallic = 1.0f;
        f32 Roughness = 1.0f;
        f32 Occlusion = 1.0f;
        std::array<Handle<TextureData>, static_cast<size_t>(MaterialTexture::Count)> Textures{};
    };

} // namespace VKE::Assets

namespace std {
    template<>
    struct hash<VKE::Assets::AssetID> {
        size_t operator()(const VKE::Assets::AssetID& id) const { return hash<VKE::u64>()(id.Value); }
    };
} // namespace std
