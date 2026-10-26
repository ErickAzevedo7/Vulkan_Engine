#pragma once

#include <assets/AssetTypes.h>
#include <core/Types.h>

#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace VKE::Assets {

    enum class AssetType : u8 {
        Model,
        Texture,
        Material,
    };

    // A texture's color space setting in its .vkasset.
    enum class TextureColorSpace : u8 {
        // Whatever the material or model using it asks for: sRGB for color, linear for data.
        FromUse,
        SRGB,
        Linear,
    };

    // What a .vkasset holds about one asset file.
    struct AssetEntry {
        AssetID ID;
        AssetType Type = AssetType::Texture;
        // Relative to the asset root, with forward slashes.
        std::string Path;
        TextureColorSpace ColorSpace = TextureColorSpace::FromUse;
        // Models: the meshes and materials inside the file, by their names there.
        std::map<std::string, AssetID> Meshes;
        std::map<std::string, AssetID> Materials;
    };

    enum class SubAssetKind : u8 {
        Mesh,
        Material,
    };

    // A mesh or material inside a model file.
    struct SubAssetEntry {
        AssetID Model;
        SubAssetKind Kind = SubAssetKind::Mesh;
        std::string Name;
    };

    // The asset files under the asset root and their IDs, read from the .vkasset next to each.
    // A file without one gets a new ID, written to a new .vkasset.
    class AssetCatalog {
    public:
        explicit AssetCatalog(std::filesystem::path root);

        // Reads every asset file under the root.
        void Scan();

        // Null if the file isn't an asset under the root. With `addIfMissing`, a file the scan
        // didn't see, like one created since, is added.
        const AssetEntry* FindByFile(const std::filesystem::path& file, bool addIfMissing);
        const AssetEntry* Find(AssetID id) const;
        const SubAssetEntry* FindSubAsset(AssetID id) const;
        std::filesystem::path GetFile(const AssetEntry& entry) const { return m_Root / entry.Path; }

        // The IDs of a model's meshes and materials, in the order of the names given. Names seen
        // before keep their IDs; names no longer in the file are dropped from the .vkasset.
        void AssignSubAssets(AssetID model, const std::vector<std::string>& meshNames,
                             const std::vector<std::string>& materialNames, std::vector<AssetID>& outMeshIDs,
                             std::vector<AssetID>& outMaterialIDs);

    private:
        const AssetEntry* Add(const std::filesystem::path& file, AssetType type);
        // Returns false if the file is missing or unreadable.
        bool ReadMetadata(const std::filesystem::path& metadataFile, AssetEntry& entry) const;
        void WriteMetadata(const AssetEntry& entry) const;
        // An ID no other asset or sub-asset uses.
        AssetID MakeUnique(AssetID wanted) const;
        bool IsTaken(AssetID id) const;
        std::string MakePathKey(const std::filesystem::path& file) const;

        std::filesystem::path m_Root;
        std::unordered_map<AssetID, AssetEntry> m_Entries;
        // Lowercased on Windows, where paths ignore case.
        std::unordered_map<std::string, AssetID> m_ByPath;
        std::unordered_map<AssetID, SubAssetEntry> m_SubAssets;
    };

} // namespace VKE::Assets
