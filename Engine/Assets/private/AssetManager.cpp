#include "assets/AssetManager.h"
#include "AssetCatalog.h"
#include "importers/Importers.h"

#include <core/Log.h>

#include <unordered_set>

namespace VKE::Assets {

    namespace {
        TextureData MakeSolidTexture(const char* name, u8 r, u8 g, u8 b) {
            TextureData texture;
            texture.Name = name;
            texture.Width = 1;
            texture.Height = 1;
            texture.ColorSpace = ColorSpace::Linear;
            texture.Pixels = {r, g, b, 255};
            return texture;
        }

        // Names inside a model file can repeat; later ones get "#2", "#3", ... in the order seen.
        std::vector<std::string> MakeUniqueNames(const std::vector<std::string>& names, const char* fallback) {
            std::unordered_set<std::string> taken;
            std::vector<std::string> unique;
            unique.reserve(names.size());
            for (const std::string& name : names) {
                std::string base = name.empty() ? fallback : name;
                std::string candidate = base;
                for (u32 suffix = 2; taken.count(candidate); ++suffix) {
                    candidate = base + "#" + std::to_string(suffix);
                }
                taken.insert(candidate);
                unique.push_back(std::move(candidate));
            }
            return unique;
        }

        template<typename T>
        Handle<T> FindLoaded(const std::unordered_map<AssetID, Handle<T>>& loaded, AssetID id) {
            auto it = loaded.find(id);
            return it != loaded.end() ? it->second : Handle<T>{};
        }
    } // namespace

    AssetManager::AssetManager(std::vector<AssetRoot> roots)
        : m_Catalog(MakeIunique<AssetCatalog>(std::move(roots))) {
        CreateDefaultTextures();
    }

    AssetManager::~AssetManager() = default;

    void AssetManager::OnInitialize() {
        m_Catalog->Scan();
    }

    const std::vector<AssetRoot>& AssetManager::GetRoots() const {
        return m_Catalog->GetRoots();
    }

    const std::filesystem::path& AssetManager::GetAssetRoot() const {
        static const std::filesystem::path kNone;
        return GetRoots().empty() ? kNone : GetRoots().front().Folder;
    }

    std::filesystem::path AssetManager::Resolve(const std::string& path) const {
        std::filesystem::path asPath = std::filesystem::u8path(path);
        return asPath.is_absolute() ? asPath : GetAssetRoot() / asPath;
    }

    AssetID AssetManager::FindAsset(const std::string& path) {
        std::filesystem::path file = Resolve(path);
        const AssetEntry* entry = m_Catalog->FindByFile(file, true);
        if (!entry) {
            std::error_code ec;
            if (!std::filesystem::exists(file, ec)) {
                VKE_CORE_ERROR("AssetManager: '{}' doesn't exist", file.u8string());
            } else {
                VKE_CORE_ERROR("AssetManager: '{}' isn't an asset under an asset root", path);
            }
            return {};
        }
        return entry->ID;
    }

    std::filesystem::path AssetManager::GetFile(AssetID id) const {
        const AssetEntry* entry = m_Catalog->Find(id);
        return entry ? m_Catalog->GetFile(*entry) : std::filesystem::path();
    }

    std::optional<AssetType> AssetManager::GetType(AssetID id) const {
        const AssetEntry* entry = m_Catalog->Find(id);
        return entry ? std::optional<AssetType>(entry->Type) : std::nullopt;
    }

    std::optional<AssetType> AssetManager::GetAnyType(AssetID id) const {
        if (const SubAssetEntry* subAsset = m_Catalog->FindSubAsset(id)) {
            return subAsset->Kind == SubAssetKind::Mesh ? AssetType::Mesh : AssetType::Material;
        }
        return GetType(id);
    }

    AssetID AssetManager::GetAsset(const std::filesystem::path& file) const {
        const AssetEntry* entry = m_Catalog->Find(file);
        return entry ? entry->ID : AssetID{};
    }

    const AssetRoot* AssetManager::FindRoot(const std::filesystem::path& file) const {
        return m_Catalog->FindRoot(file);
    }

    bool AssetManager::IsWritable(AssetID id) const {
        const AssetEntry* entry = m_Catalog->Find(id);
        return entry && !GetRoots()[entry->Root].ReadOnly;
    }

    std::vector<SubAsset> AssetManager::GetSubAssets(AssetID model) const {
        std::vector<SubAsset> subAssets;
        const AssetEntry* entry = m_Catalog->Find(model);
        if (!entry) return subAssets;
        for (const auto& [name, id] : entry->Meshes) {
            subAssets.push_back({id, AssetType::Mesh, name});
        }
        for (const auto& [name, id] : entry->Materials) {
            subAssets.push_back({id, AssetType::Material, name});
        }
        return subAssets;
    }

    bool AssetManager::Refresh() {
        return m_Catalog->Refresh();
    }

    bool AssetManager::Move(const std::filesystem::path& from, const std::filesystem::path& to) {
        const AssetRoot* fromRoot = FindRoot(from);
        const AssetRoot* toRoot = FindRoot(to);
        if (!fromRoot || !toRoot || fromRoot->ReadOnly || toRoot->ReadOnly) {
            VKE_CORE_ERROR("AssetManager: '{}' can't be moved to '{}': both must be in a writable asset root",
                           from.u8string(), to.u8string());
            return false;
        }
        std::error_code ec;
        if (std::filesystem::exists(to, ec)) {
            VKE_CORE_ERROR("AssetManager: '{}' already exists", to.u8string());
            return false;
        }

        bool isFile = std::filesystem::is_regular_file(from, ec);
        AssetID moved = isFile ? GetAsset(from) : AssetID{};
        std::filesystem::rename(from, to, ec);
        if (ec) {
            VKE_CORE_ERROR("AssetManager: couldn't move '{}' to '{}': {}", from.u8string(), to.u8string(),
                           ec.message());
            return false;
        }
        // A file's .vkasset goes with it; a folder's files take theirs along already.
        if (isFile) {
            std::filesystem::path metadata = from;
            metadata += ".vkasset";
            std::filesystem::path movedMetadata = to;
            movedMetadata += ".vkasset";
            if (std::filesystem::exists(metadata, ec)) std::filesystem::rename(metadata, movedMetadata, ec);
            if (ec) VKE_CORE_WARN("AssetManager: couldn't move '{}': {}", metadata.u8string(), ec.message());
        }
        m_Catalog->Moved(from, to);

        // A material named after its file follows it.
        if (MaterialData* material = m_Materials.Get(FindLoaded(m_LoadedMaterials, moved))) {
            if (material->Name == from.stem().u8string()) material->Name = to.stem().u8string();
        }
        return true;
    }

    bool AssetManager::Delete(const std::filesystem::path& path) {
        const AssetRoot* root = FindRoot(path);
        if (!root || root->ReadOnly) {
            VKE_CORE_ERROR("AssetManager: '{}' can't be deleted: it isn't in a writable asset root", path.u8string());
            return false;
        }
        std::error_code ec;
        bool isFile = std::filesystem::is_regular_file(path, ec);
        std::filesystem::remove_all(path, ec);
        if (!ec && isFile) {
            std::filesystem::path metadata = path;
            metadata += ".vkasset";
            std::filesystem::remove(metadata, ec);
        }
        if (ec) {
            VKE_CORE_ERROR("AssetManager: couldn't delete '{}': {}", path.u8string(), ec.message());
            return false;
        }
        m_Catalog->Removed(path);
        return true;
    }

    Handle<ModelData> AssetManager::LoadModel(AssetID id) {
        if (auto it = m_LoadedModels.find(id); it != m_LoadedModels.end()) return it->second;

        const AssetEntry* entry = m_Catalog->Find(id);
        if (!entry || entry->Type != AssetType::Model) {
            if (id.IsValid()) VKE_CORE_ERROR("AssetManager: no model has the ID {}", id.ToString());
            return {};
        }

        ModelData model;
        std::filesystem::path file = m_Catalog->GetFile(*entry);
        if (!Importers::ImportModel(*this, file, model)) {
            VKE_CORE_ERROR("AssetManager: failed to load model '{}'", entry->Path);
            return {};
        }

        size_t meshCount = 0;
        size_t vertexCount = 0;
        for (const ModelNode& node : model.Nodes) {
            if (const MeshData* mesh = GetMesh(node.Mesh)) {
                ++meshCount;
                vertexCount += mesh->Vertices.size();
            }
        }
        VKE_CORE_INFO("Loaded model '{}': {} nodes, {} meshes on nodes, {} vertices", entry->Path, model.Nodes.size(),
                      meshCount, vertexCount);

        AssignSubAssets(id, model);
        model.ID = id;
        Handle<ModelData> handle = m_Models.Emplace(std::move(model));
        m_LoadedModels[id] = handle;
        return handle;
    }

    void AssetManager::AssignSubAssets(AssetID id, const ModelData& model) {
        // Each mesh once, in node order, and each material once, in the order the meshes use them.
        std::vector<Handle<MeshData>> meshes;
        std::vector<Handle<MaterialData>> materials;
        std::unordered_set<Handle<MeshData>> seenMeshes;
        std::unordered_set<Handle<MaterialData>> seenMaterials;
        for (const ModelNode& node : model.Nodes) {
            const MeshData* mesh = GetMesh(node.Mesh);
            if (!mesh || !seenMeshes.insert(node.Mesh).second) continue;
            meshes.push_back(node.Mesh);
            for (Handle<MaterialData> material : mesh->Materials) {
                if (seenMaterials.insert(material).second) materials.push_back(material);
            }
        }

        std::vector<std::string> meshNames;
        for (Handle<MeshData> mesh : meshes) {
            meshNames.push_back(GetMesh(mesh)->Name);
        }
        std::vector<std::string> materialNames;
        for (Handle<MaterialData> material : materials) {
            const MaterialData* data = GetMaterial(material);
            materialNames.push_back(data ? data->Name : std::string());
        }

        std::vector<AssetID> meshIDs;
        std::vector<AssetID> materialIDs;
        m_Catalog->AssignSubAssets(id, MakeUniqueNames(meshNames, "Mesh"), MakeUniqueNames(materialNames, "Material"),
                                   meshIDs, materialIDs);
        for (size_t i = 0; i < meshes.size(); ++i) {
            m_LoadedMeshes[meshIDs[i]] = meshes[i];
            m_Meshes.Get(meshes[i])->ID = meshIDs[i];
        }
        for (size_t i = 0; i < materials.size(); ++i) {
            m_LoadedMaterials[materialIDs[i]] = materials[i];
            if (MaterialData* material = m_Materials.Get(materials[i])) material->ID = materialIDs[i];
        }
    }

    Handle<TextureData> AssetManager::LoadTexture(AssetID id, ColorSpace use) {
        const AssetEntry* entry = m_Catalog->Find(id);
        if (!entry || entry->Type != AssetType::Texture) {
            if (id.IsValid()) VKE_CORE_ERROR("AssetManager: no texture has the ID {}", id.ToString());
            return {};
        }

        ColorSpace colorSpace = entry->ColorSpace == TextureColorSpace::SRGB     ? ColorSpace::SRGB
                                : entry->ColorSpace == TextureColorSpace::Linear ? ColorSpace::Linear
                                                                                 : use;
        // The same image is a different texture in each color space.
        Handle<TextureData>& slot = m_LoadedTextures[id][colorSpace == ColorSpace::SRGB ? 1 : 0];
        if (m_Textures.Contains(slot)) return slot;

        TextureData texture;
        if (!Importers::ImportTexture(m_Catalog->GetFile(*entry), colorSpace, texture)) {
            VKE_CORE_ERROR("AssetManager: failed to load texture '{}'", entry->Path);
            return {};
        }

        slot = AddTexture(std::move(texture));
        m_Textures.Get(slot)->ID = id;
        return slot;
    }

    Handle<MaterialData> AssetManager::LoadMaterial(AssetID id) {
        if (auto it = m_LoadedMaterials.find(id); it != m_LoadedMaterials.end()) return it->second;

        // A material inside a model comes with the model.
        if (const SubAssetEntry* subAsset = m_Catalog->FindSubAsset(id)) {
            if (subAsset->Kind != SubAssetKind::Material) return {};
            LoadModel(subAsset->Model);
            auto it = m_LoadedMaterials.find(id);
            return it != m_LoadedMaterials.end() ? it->second : Handle<MaterialData>{};
        }

        const AssetEntry* entry = m_Catalog->Find(id);
        if (!entry || entry->Type != AssetType::Material) {
            if (id.IsValid()) VKE_CORE_ERROR("AssetManager: no material has the ID {}", id.ToString());
            return {};
        }

        MaterialData material;
        if (!Importers::ImportMaterial(*this, m_Catalog->GetFile(*entry), material)) {
            VKE_CORE_ERROR("AssetManager: failed to load material '{}'", entry->Path);
            return {};
        }

        Handle<MaterialData> handle = AddMaterial(std::move(material));
        m_Materials.Get(handle)->ID = id;
        m_LoadedMaterials[id] = handle;
        return handle;
    }

    Handle<MeshData> AssetManager::LoadMesh(AssetID id) {
        if (auto it = m_LoadedMeshes.find(id); it != m_LoadedMeshes.end()) return it->second;

        const SubAssetEntry* subAsset = m_Catalog->FindSubAsset(id);
        if (!subAsset || subAsset->Kind != SubAssetKind::Mesh) {
            if (id.IsValid()) VKE_CORE_ERROR("AssetManager: no mesh has the ID {}", id.ToString());
            return {};
        }

        LoadModel(subAsset->Model);
        auto it = m_LoadedMeshes.find(id);
        return it != m_LoadedMeshes.end() ? it->second : Handle<MeshData>{};
    }

    Handle<TextureData> AssetManager::FindLoadedTexture(AssetID id) const {
        auto it = m_LoadedTextures.find(id);
        if (it == m_LoadedTextures.end()) return {};
        for (Handle<TextureData> texture : it->second) {
            if (m_Textures.Contains(texture)) return texture;
        }
        return {};
    }

    TextureColorSpace AssetManager::GetColorSpace(AssetID texture) const {
        const AssetEntry* entry = m_Catalog->Find(texture);
        return entry ? entry->ColorSpace : TextureColorSpace::FromUse;
    }

    bool AssetManager::SetColorSpace(AssetID texture, TextureColorSpace colorSpace) {
        if (!m_Catalog->SetColorSpace(texture, colorSpace)) return false;

        // Materials holding the texture take it again, in the space they read it in now.
        for (auto& [id, handle] : m_LoadedMaterials) {
            MaterialData* material = m_Materials.Get(handle);
            if (!material) continue;
            bool changed = false;
            for (u32 slot = 0; slot < static_cast<u32>(MaterialTexture::Count); ++slot) {
                Handle<TextureData>& used = material->Textures[slot];
                if (GetID(used) != texture) continue;
                MaterialTexture use = static_cast<MaterialTexture>(slot);
                Handle<TextureData> loaded = LoadTexture(texture, GetSlotColorSpace(use));
                if (!loaded.IsValid() || loaded == used) continue;
                used = loaded;
                changed = true;
            }
            if (changed) ++material->Revision;
        }
        return true;
    }

    AssetID AssetManager::CreateMaterial(const std::filesystem::path& file) {
        const AssetRoot* root = FindRoot(file);
        std::error_code ec;
        if (!root || root->ReadOnly) {
            VKE_CORE_ERROR("AssetManager: '{}' isn't in a writable asset root", file.u8string());
            return {};
        }
        if (std::filesystem::exists(file, ec)) {
            VKE_CORE_ERROR("AssetManager: '{}' already exists", file.u8string());
            return {};
        }
        // Content/Materials/Default.mat's values.
        MaterialData material;
        material.BaseColor = Vec4(0.55f, 0.55f, 0.55f, 1.0f);
        material.Metallic = 0.0f;
        material.Roughness = 0.5f;
        if (!Importers::ExportMaterial(*this, material, file)) return {};
        return FindAsset(file.u8string());
    }

    void AssetManager::UpdateMaterial(Handle<MaterialData> handle, MaterialData material) {
        MaterialData* current = m_Materials.Get(handle);
        if (!current) return;
        FillDefaultTextures(material);
        material.ID = current->ID;
        material.Revision = current->Revision + 1;
        *current = std::move(material);
    }

    bool AssetManager::SaveMaterial(AssetID id) {
        const MaterialData* material = GetMaterial(FindLoaded(m_LoadedMaterials, id));
        if (!material || GetType(id) != AssetType::Material || !IsWritable(id)) return false;
        return Importers::ExportMaterial(*this, *material, GetFile(id));
    }

    bool AssetManager::ReloadMaterial(AssetID id) {
        Handle<MaterialData> handle = FindLoaded(m_LoadedMaterials, id);
        if (!m_Materials.Contains(handle) || GetType(id) != AssetType::Material) return false;
        MaterialData material;
        if (!Importers::ImportMaterial(*this, GetFile(id), material)) return false;
        UpdateMaterial(handle, std::move(material));
        return true;
    }

    AssetID AssetManager::GetID(Handle<ModelData> handle) const {
        const ModelData* model = GetModel(handle);
        return model ? model->ID : AssetID{};
    }

    AssetID AssetManager::GetID(Handle<MeshData> handle) const {
        const MeshData* mesh = GetMesh(handle);
        return mesh ? mesh->ID : AssetID{};
    }

    AssetID AssetManager::GetID(Handle<TextureData> handle) const {
        const TextureData* texture = GetTexture(handle);
        return texture ? texture->ID : AssetID{};
    }

    AssetID AssetManager::GetID(Handle<MaterialData> handle) const {
        const MaterialData* material = GetMaterial(handle);
        return material ? material->ID : AssetID{};
    }

    Handle<MeshData> AssetManager::AddMesh(MeshData mesh) {
        mesh.ID = {};
        // Geometry is only drawn through submeshes, so a mesh with none gets one covering it all.
        if (mesh.Submeshes.empty() && !mesh.Indices.empty()) {
            Submesh submesh;
            submesh.IndexCount = static_cast<u32>(mesh.Indices.size());
            mesh.Submeshes.push_back(submesh);
        }
        return m_Meshes.Emplace(std::move(mesh));
    }

    Handle<TextureData> AssetManager::AddTexture(TextureData texture) {
        texture.ID = {};
        return m_Textures.Emplace(std::move(texture));
    }

    Handle<MaterialData> AssetManager::AddMaterial(MaterialData material) {
        material.ID = {};
        FillDefaultTextures(material);
        return m_Materials.Emplace(std::move(material));
    }

    void AssetManager::FillDefaultTextures(MaterialData& material) const {
        auto fill = [&](MaterialTexture slot, DefaultTexture fallback) {
            Handle<TextureData>& texture = material.Textures[static_cast<size_t>(slot)];
            if (!m_Textures.Contains(texture)) texture = GetDefaultTexture(fallback);
        };
        fill(MaterialTexture::Albedo, DefaultTexture::White);
        fill(MaterialTexture::Normal, DefaultTexture::FlatNormal);
        fill(MaterialTexture::Metallic, DefaultTexture::White);
        fill(MaterialTexture::Roughness, DefaultTexture::White);
        fill(MaterialTexture::Occlusion, DefaultTexture::White);
        fill(MaterialTexture::Emissive, DefaultTexture::White);
    }

    void AssetManager::CreateDefaultTextures() {
        m_DefaultTextures[static_cast<size_t>(DefaultTexture::White)] = AddTexture(MakeSolidTexture("$white", 255, 255, 255));
        m_DefaultTextures[static_cast<size_t>(DefaultTexture::Black)] = AddTexture(MakeSolidTexture("$black", 0, 0, 0));
        m_DefaultTextures[static_cast<size_t>(DefaultTexture::FlatNormal)] =
            AddTexture(MakeSolidTexture("$flat_normal", 128, 128, 255));
    }

} // namespace VKE::Assets
