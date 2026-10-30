#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/SlotMap.h>
#include <core/System.h>
#include <array>
#include <filesystem>
#include <optional>
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

    // Loads assets from disk into memory and hands out handles to them. Asset files live under
    // asset roots (the project's assets, the engine's content), each with a .vkasset next to it
    // that holds its ID; the roots are scanned when the manager starts, and files without a
    // .vkasset in a writable root get one. Loading the same asset again returns the existing handle.
    class AssetManager : public System {
    public:
        // Relative asset paths are resolved against the first root.
        explicit AssetManager(std::vector<AssetRoot> roots);
        ~AssetManager() override;

        void OnInitialize() override;
        std::string_view GetName() const override { return "AssetManager"; }

        const std::vector<AssetRoot>& GetRoots() const;
        // The first root's folder, where relative paths point.
        const std::filesystem::path& GetAssetRoot() const;

        // File path for an asset path. Absolute paths are returned unchanged.
        std::filesystem::path Resolve(const std::string& path) const;

        // The ID of the asset file at `path`, relative to the first root or absolute inside any.
        // A file created since the scan is added. Invalid if the file isn't an asset under a root.
        AssetID FindAsset(const std::string& path);

        // The ID of the asset file at `file`, if the catalog has it; unlike FindAsset, nothing is
        // added or logged.
        AssetID GetAsset(const std::filesystem::path& file) const;

        // The file and type of an asset; empty and nothing for IDs that aren't asset files (the
        // meshes and materials inside a model have no file of their own).
        std::filesystem::path GetFile(AssetID id) const;
        std::optional<AssetType> GetType(AssetID id) const;
        // The type of an asset file or of a mesh or material inside a model.
        std::optional<AssetType> GetAnyType(AssetID id) const;
        // The root `file` is under, or null.
        const AssetRoot* FindRoot(const std::filesystem::path& file) const;
        // False for assets in a read-only root and for IDs that aren't asset files.
        bool IsWritable(AssetID id) const;

        // The meshes and materials a model's .vkasset lists, by name; a model never loaded may
        // list none yet.
        std::vector<SubAsset> GetSubAssets(AssetID model) const;

        // Adds the asset files created since the roots were scanned and drops the ones whose files
        // are gone; what was loaded from them stays loaded. Returns whether anything changed.
        bool Refresh();
        // Moves or renames a file or folder inside a writable root, with the .vkasset files, so
        // the assets keep their IDs. False, logged, if it can't.
        bool Move(const std::filesystem::path& from, const std::filesystem::path& to);
        // Deletes a file or folder inside a writable root, with the .vkasset files. What was
        // loaded from it stays loaded until the program ends. False, logged, if it can't.
        bool Delete(const std::filesystem::path& path);

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

        // The texture loaded from the asset, in either color space, or invalid if it wasn't loaded.
        Handle<TextureData> FindLoadedTexture(AssetID id) const;
        TextureColorSpace GetColorSpace(AssetID texture) const;
        // Writes the setting to the texture's .vkasset and loads the texture again in the space
        // each loaded material reads it in now. False for read-only textures.
        bool SetColorSpace(AssetID texture, TextureColorSpace colorSpace);

        // Writes a .mat file with the default material's values and returns its ID; invalid,
        // logged, if it can't.
        AssetID CreateMaterial(const std::filesystem::path& file);
        // Replaces a loaded material's values, keeping its handle and ID; the renderer picks the
        // change up. Empty texture slots get default textures.
        void UpdateMaterial(Handle<MaterialData> handle, MaterialData material);
        // Writes a loaded .mat material's values to its file.
        bool SaveMaterial(AssetID id);
        // Reads a loaded .mat material's file again, dropping the changes made since.
        bool ReloadMaterial(AssetID id);

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
        void FillDefaultTextures(MaterialData& material) const;
        // Gives IDs to the meshes and materials of a model just imported from the asset `id`.
        void AssignSubAssets(AssetID id, const ModelData& model);

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
