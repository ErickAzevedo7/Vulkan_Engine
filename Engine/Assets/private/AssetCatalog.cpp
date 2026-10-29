#include "AssetCatalog.h"
#include "YamlUtils.h"

#include <core/Log.h>

#include <algorithm>
#include <cctype>
#include <fstream>

namespace VKE::Assets {

    namespace {
        constexpr const char* kMetadataExtension = ".vkasset";

        bool FindType(const std::filesystem::path& file, AssetType& outType) {
            std::string extension = file.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (extension == ".gltf" || extension == ".glb" || extension == ".obj" || extension == ".fbx" ||
                extension == ".dae") {
                outType = AssetType::Model;
            } else if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga" ||
                       extension == ".bmp" || extension == ".hdr") {
                outType = AssetType::Texture;
            } else if (extension == ".mat") {
                outType = AssetType::Material;
            } else if (extension == ".scene") {
                outType = AssetType::Scene;
            } else {
                return false;
            }
            return true;
        }

        std::filesystem::path GetMetadataFile(const std::filesystem::path& file) {
            std::filesystem::path metadata = file;
            metadata += kMetadataExtension;
            return metadata;
        }

        const char* ToString(AssetType type) {
            switch (type) {
            case AssetType::Model: return "Model";
            case AssetType::Texture: return "Texture";
            case AssetType::Material: return "Material";
            case AssetType::Scene: return "Scene";
            }
            return "";
        }

        const char* ToString(TextureColorSpace colorSpace) {
            switch (colorSpace) {
            case TextureColorSpace::FromUse: return "FromUse";
            case TextureColorSpace::SRGB: return "SRGB";
            case TextureColorSpace::Linear: return "Linear";
            }
            return "";
        }

        TextureColorSpace ParseColorSpace(const std::string& text) {
            if (text == "SRGB") return TextureColorSpace::SRGB;
            if (text == "Linear") return TextureColorSpace::Linear;
            return TextureColorSpace::FromUse;
        }

        void ReadSubAssets(const YAML::Node& node, std::map<std::string, AssetID>& out) {
            if (!node || !node.IsMap()) return;
            for (const auto& item : node) {
                try {
                    AssetID id = AssetID::Parse(item.second.as<std::string>());
                    if (id.IsValid()) out[item.first.as<std::string>()] = id;
                } catch (const YAML::Exception&) {
                }
            }
        }

        void WriteSubAssets(YAML::Emitter& out, const char* key, const std::map<std::string, AssetID>& subAssets) {
            if (subAssets.empty()) return;
            out << YAML::Key << key << YAML::Value << YAML::BeginMap;
            for (const auto& [name, id] : subAssets) {
                out << YAML::Key << YAML::DoubleQuoted << name << YAML::Value << YAML::DoubleQuoted << id.ToString();
            }
            out << YAML::EndMap;
        }
    } // namespace

    AssetCatalog::AssetCatalog(std::vector<AssetRoot> roots) : m_Roots(std::move(roots)) {
        for (AssetRoot& root : m_Roots) {
            std::error_code ec;
            std::filesystem::path canonical = std::filesystem::weakly_canonical(root.Folder, ec);
            if (!ec) root.Folder = std::move(canonical);
        }
    }

    void AssetCatalog::Scan() {
        auto options = std::filesystem::directory_options::skip_permission_denied;
        for (const AssetRoot& root : m_Roots) {
            std::error_code ec;
            if (!std::filesystem::is_directory(root.Folder, ec)) {
                VKE_CORE_WARN("AssetCatalog: asset root '{}' does not exist", root.Folder.string());
                continue;
            }

            size_t created = m_Entries.size();
            for (auto it = std::filesystem::recursive_directory_iterator(root.Folder, options, ec);
                 !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
                AssetType type;
                if (!it->is_regular_file(ec) || !FindType(it->path(), type)) continue;
                if (m_ByPath.count(MakePathKey(it->path()))) continue;
                Add(it->path(), type);
            }
            VKE_CORE_INFO("AssetCatalog: {} assets under '{}'{}", m_Entries.size() - created, root.Folder.string(),
                          root.ReadOnly ? " (read-only)" : "");
        }
    }

    const AssetEntry* AssetCatalog::FindByFile(const std::filesystem::path& file, bool addIfMissing) {
        std::string key = MakePathKey(file);
        if (key.empty()) return nullptr;
        if (auto it = m_ByPath.find(key); it != m_ByPath.end()) return &m_Entries.at(it->second);

        std::error_code ec;
        AssetType type;
        if (!addIfMissing || !FindType(file, type) || !std::filesystem::is_regular_file(file, ec)) return nullptr;
        return Add(file, type);
    }

    const AssetEntry* AssetCatalog::Find(AssetID id) const {
        auto it = m_Entries.find(id);
        return it != m_Entries.end() ? &it->second : nullptr;
    }

    const SubAssetEntry* AssetCatalog::FindSubAsset(AssetID id) const {
        auto it = m_SubAssets.find(id);
        return it != m_SubAssets.end() ? &it->second : nullptr;
    }

    void AssetCatalog::AssignSubAssets(AssetID model, const std::vector<std::string>& meshNames,
                                       const std::vector<std::string>& materialNames, std::vector<AssetID>& outMeshIDs,
                                       std::vector<AssetID>& outMaterialIDs) {
        AssetEntry& entry = m_Entries.at(model);

        auto assign = [&](SubAssetKind kind, const std::vector<std::string>& names,
                          std::map<std::string, AssetID>& subAssets, std::vector<AssetID>& outIDs) {
            std::map<std::string, AssetID> updated;
            outIDs.clear();
            for (const std::string& name : names) {
                auto existing = subAssets.find(name);
                AssetID id = existing != subAssets.end() ? existing->second : MakeUnique(AssetID::Generate());
                updated[name] = id;
                m_SubAssets[id] = {model, kind, name};
                outIDs.push_back(id);
            }
            for (const auto& [name, id] : subAssets) {
                if (!updated.count(name)) m_SubAssets.erase(id);
            }

            bool changed = updated != subAssets;
            subAssets = std::move(updated);
            return changed;
        };

        bool meshesChanged = assign(SubAssetKind::Mesh, meshNames, entry.Meshes, outMeshIDs);
        bool materialsChanged = assign(SubAssetKind::Material, materialNames, entry.Materials, outMaterialIDs);
        if (!meshesChanged && !materialsChanged) return;

        const AssetRoot& root = m_Roots[entry.Root];
        if (root.ReadOnly) {
            VKE_CORE_WARN("AssetCatalog: the .vkasset of '{}' in '{}' doesn't list the meshes and materials the file "
                          "has; the ones it misses get IDs that only last this run",
                          entry.Path, root.Name);
            return;
        }
        WriteMetadata(entry);
    }

    const AssetEntry* AssetCatalog::Add(const std::filesystem::path& file, AssetType type) {
        std::string key = MakePathKey(file);
        if (key.empty()) return nullptr;

        AssetEntry entry;
        entry.Type = type;
        // Not the key, which is lowercased on Windows: the entry keeps the name as it is on disk.
        FindRoot(file, entry.Root, entry.Path);
        const AssetRoot& root = m_Roots[entry.Root];

        bool hasMetadata = ReadMetadata(GetMetadataFile(file), entry);
        // A read-only root can't be given new IDs, and an ID that changed every run would break
        // whatever refers to the asset.
        if (root.ReadOnly && (!entry.ID.IsValid() || IsTaken(entry.ID))) {
            VKE_CORE_WARN("AssetCatalog: '{}' in '{}' is left out: {}", entry.Path, root.Name,
                          entry.ID.IsValid() ? "its ID is another asset's" : "it has no ID in a .vkasset");
            return nullptr;
        }

        bool write = !hasMetadata;
        if (!entry.ID.IsValid() || IsTaken(entry.ID)) {
            // A copied file brings its copy of the original's .vkasset, with the same ID.
            if (entry.ID.IsValid()) {
                VKE_CORE_WARN("AssetCatalog: '{}' has the ID of another asset and gets a new one", entry.Path);
            }
            entry.ID = MakeUnique(AssetID::Generate());
            write = true;
        }

        for (auto* subAssets : {&entry.Meshes, &entry.Materials}) {
            SubAssetKind kind = subAssets == &entry.Meshes ? SubAssetKind::Mesh : SubAssetKind::Material;
            for (auto& [name, id] : *subAssets) {
                if (IsTaken(id)) {
                    if (root.ReadOnly) {
                        VKE_CORE_WARN("AssetCatalog: '{}' in '{}' of '{}' has another asset's ID; its new one only "
                                      "lasts this run",
                                      name, entry.Path, root.Name);
                    }
                    id = MakeUnique(AssetID::Generate());
                    write = true;
                }
                m_SubAssets[id] = {entry.ID, kind, name};
            }
        }

        if (write && !root.ReadOnly) WriteMetadata(entry);

        AssetID id = entry.ID;
        m_ByPath[key] = id;
        return &(m_Entries[id] = std::move(entry));
    }

    bool AssetCatalog::ReadMetadata(const std::filesystem::path& metadataFile, AssetEntry& entry) const {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(metadataFile, ec)) return false;

        YAML::Node root;
        try {
            root = YAML::LoadFile(metadataFile.string());
        } catch (const YAML::Exception& error) {
            VKE_CORE_ERROR("AssetCatalog: '{}': {}", metadataFile.string(), error.what());
            return false;
        }
        if (!root.IsMap()) return false;

        entry.ID = AssetID::Parse(ReadYaml<std::string>(root, "ID", std::string()));
        if (entry.Type == AssetType::Texture) {
            entry.ColorSpace = ParseColorSpace(ReadYaml<std::string>(root, "ColorSpace", std::string()));
        }
        if (entry.Type == AssetType::Model) {
            ReadSubAssets(root["Meshes"], entry.Meshes);
            ReadSubAssets(root["Materials"], entry.Materials);
        }
        return true;
    }

    void AssetCatalog::WriteMetadata(const AssetEntry& entry) const {
        YAML::Emitter out;
        out << YAML::Comment("Written by the engine. Keep it next to its asset when moving or renaming it.");
        out << YAML::BeginMap;
        out << YAML::Key << "ID" << YAML::Value << YAML::DoubleQuoted << entry.ID.ToString();
        out << YAML::Key << "Type" << YAML::Value << ToString(entry.Type);
        if (entry.Type == AssetType::Texture) {
            out << YAML::Key << "ColorSpace" << YAML::Value << ToString(entry.ColorSpace);
        }
        WriteSubAssets(out, "Meshes", entry.Meshes);
        WriteSubAssets(out, "Materials", entry.Materials);
        out << YAML::EndMap;

        std::filesystem::path file = GetMetadataFile(GetFile(entry));
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        stream << out.c_str() << '\n';
        if (!stream) VKE_CORE_ERROR("AssetCatalog: couldn't write '{}'", file.string());
    }

    AssetID AssetCatalog::MakeUnique(AssetID wanted) const {
        while (!wanted.IsValid() || IsTaken(wanted)) {
            wanted = AssetID::Generate();
        }
        return wanted;
    }

    bool AssetCatalog::IsTaken(AssetID id) const {
        return m_Entries.count(id) || m_SubAssets.count(id);
    }

    bool AssetCatalog::FindRoot(const std::filesystem::path& file, u32& outRoot, std::string& outPath) const {
        std::error_code ec;
        std::filesystem::path canonical = std::filesystem::weakly_canonical(file, ec);
        if (ec) return false;
        for (u32 i = 0; i < m_Roots.size(); ++i) {
            std::string relative = canonical.lexically_relative(m_Roots[i].Folder).generic_string();
            // Empty on another drive; starts with ".." outside the folder.
            if (relative.empty() || relative == "." || relative.rfind("..", 0) == 0) continue;
            outRoot = i;
            outPath = std::move(relative);
            return true;
        }
        return false;
    }

    std::string AssetCatalog::MakePathKey(const std::filesystem::path& file) const {
        u32 root = 0;
        std::string path;
        if (!FindRoot(file, root, path)) return {};
        // The same relative path can be in two roots.
        std::string key = std::to_string(root) + ":" + path;
#ifdef _WIN32
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
        return key;
    }

} // namespace VKE::Assets
