#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/SlotMap.h>
#include <core/System.h>
#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace VKE::Assets {

    enum class DefaultTexture {
        White,
        Black,
        FlatNormal,
        Count,
    };

    // Loads assets from disk into memory and hands out handles to them.
    // Loading the same file again returns the existing handle.
    class AssetManager : public System {
    public:
        // Relative asset paths are resolved against `assetRoot`.
        explicit AssetManager(std::filesystem::path assetRoot);

        void OnInitialize() override;
        std::string_view GetName() const override { return "AssetManager"; }

        const std::filesystem::path& GetAssetRoot() const { return m_AssetRoot; }

        // File path for an asset path. Absolute paths are returned unchanged.
        std::filesystem::path Resolve(const std::string& path) const;

        // Each returns an invalid handle if the file can't be loaded.
        Handle<MeshData> LoadMesh(const std::string& path);
        Handle<TextureData> LoadTexture(const std::string& path, ColorSpace colorSpace);
        Handle<MaterialData> LoadMaterial(const std::string& path);

        Handle<MeshData> AddMesh(MeshData mesh);
        Handle<TextureData> AddTexture(TextureData texture);
        // Empty texture slots are filled with neutral default textures.
        Handle<MaterialData> AddMaterial(MaterialData material);

        const MeshData* GetMesh(Handle<MeshData> handle) const { return m_Meshes.Get(handle); }
        const TextureData* GetTexture(Handle<TextureData> handle) const { return m_Textures.Get(handle); }
        const MaterialData* GetMaterial(Handle<MaterialData> handle) const { return m_Materials.Get(handle); }

        Handle<TextureData> GetDefaultTexture(DefaultTexture texture) const {
            return m_DefaultTextures[static_cast<size_t>(texture)];
        }

    private:
        void CreateDefaultTextures();

        std::filesystem::path m_AssetRoot;

        SlotMap<MeshData> m_Meshes;
        SlotMap<TextureData> m_Textures;
        SlotMap<MaterialData> m_Materials;

        std::unordered_map<std::string, Handle<MeshData>> m_MeshCache;
        std::unordered_map<std::string, Handle<TextureData>> m_TextureCache;
        std::unordered_map<std::string, Handle<MaterialData>> m_MaterialCache;

        std::array<Handle<TextureData>, static_cast<size_t>(DefaultTexture::Count)> m_DefaultTextures{};
    };

} // namespace VKE::Assets
