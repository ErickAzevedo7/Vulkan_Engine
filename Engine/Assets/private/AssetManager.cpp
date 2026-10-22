#include "assets/AssetManager.h"
#include "importers/Importers.h"

#include <core/Log.h>

#include <algorithm>
#include <cctype>

namespace VKE::Assets {

    namespace {
        std::string MakeCacheKey(const std::filesystem::path& file) {
            std::error_code ec;
            std::filesystem::path canonical = std::filesystem::weakly_canonical(file, ec);
            std::string key = (ec ? file : canonical).generic_string();
#ifdef _WIN32
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
            return key;
        }

        TextureData MakeSolidTexture(const char* name, u8 r, u8 g, u8 b) {
            TextureData texture;
            texture.Name = name;
            texture.Width = 1;
            texture.Height = 1;
            texture.ColorSpace = ColorSpace::Linear;
            texture.Pixels = {r, g, b, 255};
            return texture;
        }
    } // namespace

    AssetManager::AssetManager(std::filesystem::path assetRoot)
        : m_AssetRoot(std::move(assetRoot)) {
        CreateDefaultTextures();
    }

    void AssetManager::OnInitialize() {
        std::error_code ec;
        if (!std::filesystem::is_directory(m_AssetRoot, ec)) {
            VKE_CORE_WARN("AssetManager: asset root '{}' does not exist", m_AssetRoot.string());
        } else {
            VKE_CORE_INFO("AssetManager: asset root '{}'", m_AssetRoot.string());
        }
    }

    std::filesystem::path AssetManager::Resolve(const std::string& path) const {
        std::filesystem::path asPath(path);
        return asPath.is_absolute() ? asPath : m_AssetRoot / asPath;
    }

    Handle<ModelData> AssetManager::LoadModel(const std::string& path) {
        std::filesystem::path file = Resolve(path);
        std::string key = MakeCacheKey(file);
        if (auto it = m_ModelCache.find(key); it != m_ModelCache.end() && m_Models.Contains(it->second)) {
            return it->second;
        }

        ModelData model;
        if (!Importers::ImportModel(*this, file, model)) {
            VKE_CORE_ERROR("AssetManager: failed to load model '{}'", file.string());
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
        VKE_CORE_INFO("Loaded model '{}': {} nodes, {} meshes on nodes, {} vertices", path, model.Nodes.size(),
                      meshCount, vertexCount);

        Handle<ModelData> handle = m_Models.Emplace(std::move(model));
        m_ModelCache[key] = handle;
        return handle;
    }

    Handle<TextureData> AssetManager::LoadTexture(const std::string& path, ColorSpace colorSpace) {
        std::filesystem::path file = Resolve(path);
        // The same image is a different texture in each color space.
        std::string key = MakeCacheKey(file) + (colorSpace == ColorSpace::SRGB ? "|srgb" : "|linear");
        if (auto it = m_TextureCache.find(key); it != m_TextureCache.end() && m_Textures.Contains(it->second)) {
            return it->second;
        }

        TextureData texture;
        if (!Importers::ImportTexture(file, colorSpace, texture)) {
            VKE_CORE_ERROR("AssetManager: failed to load texture '{}'", file.string());
            return {};
        }

        Handle<TextureData> handle = AddTexture(std::move(texture));
        m_TextureCache[key] = handle;
        return handle;
    }

    Handle<MaterialData> AssetManager::LoadMaterial(const std::string& path) {
        std::filesystem::path file = Resolve(path);
        std::string key = MakeCacheKey(file);
        if (auto it = m_MaterialCache.find(key); it != m_MaterialCache.end() && m_Materials.Contains(it->second)) {
            return it->second;
        }

        MaterialData material;
        if (!Importers::ImportMaterial(*this, file, material)) {
            VKE_CORE_ERROR("AssetManager: failed to load material '{}'", file.string());
            return {};
        }

        Handle<MaterialData> handle = AddMaterial(std::move(material));
        m_MaterialCache[key] = handle;
        return handle;
    }

    Handle<MeshData> AssetManager::AddMesh(MeshData mesh) {
        // Geometry is only drawn through submeshes, so a mesh with none gets one covering it all.
        if (mesh.Submeshes.empty() && !mesh.Indices.empty()) {
            Submesh submesh;
            submesh.IndexCount = static_cast<u32>(mesh.Indices.size());
            mesh.Submeshes.push_back(submesh);
        }
        return m_Meshes.Emplace(std::move(mesh));
    }

    Handle<TextureData> AssetManager::AddTexture(TextureData texture) {
        return m_Textures.Emplace(std::move(texture));
    }

    Handle<MaterialData> AssetManager::AddMaterial(MaterialData material) {
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
