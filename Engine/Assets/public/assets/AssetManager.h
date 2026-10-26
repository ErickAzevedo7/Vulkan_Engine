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

    class AssetCatalog;

    enum class DefaultTexture {
        White,
        Black,
        FlatNormal,
        Count,
    };

    // Loads assets from disk into memory and hands out handles to them. Asset files live under the
    // asset root, each with a .vkasset next to it that holds its ID; the root is scanned when the
    // manager starts, and files without a .vkasset get one. Loading the same asset again returns
    // the existing handle.
    class AssetManager : public System {
    public:
        // Relative asset paths are resolved against `assetRoot`.
        explicit AssetManager(std::filesystem::path assetRoot);
        ~AssetManager() override;

        void OnInitialize() override;
        std::string_view GetName() const override { return "AssetManager"; }

        const std::filesystem::path& GetAssetRoot() const { return m_AssetRoot; }

        // File path for an asset path. Absolute paths are returned unchanged.
        std::filesystem::path Resolve(const std::string& path) const;

        // The ID of the asset file at `path`, relative to the root or absolute inside it. A file
        // created since the scan is added. Invalid if the file isn't an asset under the root.
        AssetID FindAsset(const std::string& path);

        // Each returns an invalid handle if the asset can't be loaded.
        // A model brings the meshes and materials inside it.
        Handle<ModelData> LoadModel(AssetID id);
        // `use` is the color space the caller reads the texture in; the texture's .vkasset can
        // fix another one.
        Handle<TextureData> LoadTexture(AssetID id, ColorSpace use);
        // A .mat file, or a material inside a model.
        Handle<MaterialData> LoadMaterial(AssetID id);
        // A mesh inside a model, which is loaded with it.
        Handle<MeshData> LoadMesh(AssetID id);

        // The same, for code that knows the file.
        Handle<ModelData> LoadModel(const std::string& path) { return LoadModel(FindAsset(path)); }
        Handle<TextureData> LoadTexture(const std::string& path, ColorSpace use) {
            return LoadTexture(FindAsset(path), use);
        }
        Handle<MaterialData> LoadMaterial(const std::string& path) { return LoadMaterial(FindAsset(path)); }

        // The ID an asset was loaded from, the same as its data's ID. Invalid for assets added in
        // code, which only exist while the program runs.
        AssetID GetID(Handle<ModelData> handle) const;
        AssetID GetID(Handle<MeshData> handle) const;
        AssetID GetID(Handle<TextureData> handle) const;
        AssetID GetID(Handle<MaterialData> handle) const;

        // Assets added in code get no ID, whatever the data says.
        Handle<MeshData> AddMesh(MeshData mesh);
        Handle<TextureData> AddTexture(TextureData texture);
        // Empty texture slots are filled with neutral default textures.
        Handle<MaterialData> AddMaterial(MaterialData material);

        const ModelData* GetModel(Handle<ModelData> handle) const { return m_Models.Get(handle); }
        const MeshData* GetMesh(Handle<MeshData> handle) const { return m_Meshes.Get(handle); }
        const TextureData* GetTexture(Handle<TextureData> handle) const { return m_Textures.Get(handle); }
        const MaterialData* GetMaterial(Handle<MaterialData> handle) const { return m_Materials.Get(handle); }

        Handle<TextureData> GetDefaultTexture(DefaultTexture texture) const {
            return m_DefaultTextures[static_cast<size_t>(texture)];
        }

    private:
        void CreateDefaultTextures();
        // Gives IDs to the meshes and materials of a model just imported from the asset `id`.
        void AssignSubAssets(AssetID id, const ModelData& model);

        std::filesystem::path m_AssetRoot;
        Iunique<AssetCatalog> m_Catalog;

        SlotMap<ModelData> m_Models;
        SlotMap<MeshData> m_Meshes;
        SlotMap<TextureData> m_Textures;
        SlotMap<MaterialData> m_Materials;

        // What has been loaded from each asset.
        std::unordered_map<AssetID, Handle<ModelData>> m_LoadedModels;
        std::unordered_map<AssetID, Handle<MeshData>> m_LoadedMeshes;
        std::unordered_map<AssetID, Handle<MaterialData>> m_LoadedMaterials;
        // One texture per color space it is read in.
        std::unordered_map<AssetID, std::array<Handle<TextureData>, 2>> m_LoadedTextures;

        std::array<Handle<TextureData>, static_cast<size_t>(DefaultTexture::Count)> m_DefaultTextures{};
    };

} // namespace VKE::Assets
