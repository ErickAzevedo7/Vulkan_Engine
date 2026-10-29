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

        return m_Materials.Emplace(std::move(material));
    }

    void AssetManager::CreateDefaultTextures() {
        m_DefaultTextures[static_cast<size_t>(DefaultTexture::White)] = AddTexture(MakeSolidTexture("$white", 255, 255, 255));
        m_DefaultTextures[static_cast<size_t>(DefaultTexture::Black)] = AddTexture(MakeSolidTexture("$black", 0, 0, 0));
        m_DefaultTextures[static_cast<size_t>(DefaultTexture::FlatNormal)] =
            AddTexture(MakeSolidTexture("$flat_normal", 128, 128, 255));
    }

} // namespace VKE::Assets
