#pragma once

#include <assets/AssetTypes.h>
#include <core/Types.h>

#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace VKE::Assets {

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
        // Index of the root the file is under.
        u32 Root = 0;
        // Relative to that root, with forward slashes.
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

    // The asset files under the asset roots and their IDs, read from the .vkasset next to each.
    // IDs are unique across all roots. In a writable root, a file without a .vkasset gets a new
    // ID, written to a new .vkasset; in a read-only root it is left out.
    class AssetCatalog {
    public:
        explicit AssetCatalog(std::vector<AssetRoot> roots);

        // Reads every asset file under the roots.
        void Scan();

        const std::vector<AssetRoot>& GetRoots() const { return m_Roots; }

        // Null if the file isn't an asset under a root. With `addIfMissing`, a file the scan
        // didn't see, like one created since, is added.
        const AssetEntry* FindByFile(const std::filesystem::path& file, bool addIfMissing);
        const AssetEntry* Find(AssetID id) const;
        const SubAssetEntry* FindSubAsset(AssetID id) const;
        std::filesystem::path GetFile(const AssetEntry& entry) const { return m_Roots[entry.Root].Folder / entry.Path; }

        // The IDs of a model's meshes and materials, in the order of the names given. Names seen
        // before keep their IDs; names no longer in the file are dropped from the .vkasset. In a
        // read-only root, names the .vkasset doesn't list get IDs that only last this run.
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
        // The root `file` is under and its path relative to it, with forward slashes. False if
        // it isn't under any.
        bool FindRoot(const std::filesystem::path& file, u32& outRoot, std::string& outPath) const;
        // The key of `file` in m_ByPath; empty if it isn't under a root.
        std::string MakePathKey(const std::filesystem::path& file) const;

        std::vector<AssetRoot> m_Roots;
        std::unordered_map<AssetID, AssetEntry> m_Entries;
        // Lowercased on Windows, where paths ignore case.
        std::unordered_map<std::string, AssetID> m_ByPath;
        std::unordered_map<AssetID, SubAssetEntry> m_SubAssets;
    };

} // namespace VKE::Assets
