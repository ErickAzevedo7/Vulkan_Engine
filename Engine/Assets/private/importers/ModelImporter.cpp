#include "Importers.h"

#include <assets/AssetManager.h>
#include <core/Log.h>

#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <utility>

namespace VKE::Assets::Importers {

    namespace {
        Vec3 ToVec3(const aiVector3D& v) {
            return {v.x, v.y, v.z};
        }

        // Any unit vector perpendicular to `normal`, for meshes without UVs.
        Vec4 FallbackTangent(const Vec3& normal) {
            Vec3 axis = std::abs(normal.y) < 0.999f ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f);
            return Vec4(glm::normalize(glm::cross(axis, normal)), 1.0f);
        }

        std::string FindTexturePath(const aiMaterial& material,
                                    std::initializer_list<std::pair<aiTextureType, unsigned>> candidates) {
            for (const auto& [type, index] : candidates) {
                aiString path;
                if (material.GetTexture(type, index, &path) == aiReturn_SUCCESS && path.length > 0) {
                    return path.C_Str();
                }
            }
            return {};
        }

        Handle<TextureData> LoadTexture(AssetManager& assets, const std::filesystem::path& modelDir,
                                        const std::string& path, ColorSpace colorSpace) {
            if (path.empty()) return {};
            if (path[0] == '*') {
                VKE_CORE_WARN("ModelImporter: embedded textures are not supported ('{}')", path);
                return {};
            }

            std::filesystem::path texturePath(path);
            if (texturePath.is_relative()) texturePath = modelDir / texturePath;
            return assets.LoadTexture(texturePath.string(), colorSpace);
        }

        MaterialData ConvertMaterial(AssetManager& assets, const aiMaterial& source, const std::filesystem::path& modelDir) {
            MaterialData material;

            aiString name;
            if (source.Get(AI_MATKEY_NAME, name) == aiReturn_SUCCESS) material.Name = name.C_Str();

            aiColor4D baseColor;
            if (source.Get(AI_MATKEY_BASE_COLOR, baseColor) == aiReturn_SUCCESS ||
                source.Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == aiReturn_SUCCESS) {
                material.BaseColor = {baseColor.r, baseColor.g, baseColor.b, baseColor.a};
            }

            float metallic = 0.0f;
            source.Get(AI_MATKEY_METALLIC_FACTOR, metallic);
            material.Metallic = metallic;

            float roughness = 1.0f;
            source.Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
            material.Roughness = roughness;

            auto setTexture = [&](MaterialTexture slot, Handle<TextureData> texture) {
                material.Textures[static_cast<size_t>(slot)] = texture;
            };

            setTexture(MaterialTexture::Albedo,
                       LoadTexture(assets, modelDir,
                                   FindTexturePath(source, {{aiTextureType_BASE_COLOR, 0}, {aiTextureType_DIFFUSE, 0}}),
                                   ColorSpace::SRGB));
            setTexture(MaterialTexture::Normal,
                       LoadTexture(assets, modelDir, FindTexturePath(source, {{aiTextureType_NORMALS, 0}}),
                                   ColorSpace::Linear));
            setTexture(MaterialTexture::Metallic,
                       LoadTexture(assets, modelDir,
                                   FindTexturePath(source, {{aiTextureType_METALNESS, 0},
                                                            {AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE}}),
                                   ColorSpace::Linear));
            setTexture(MaterialTexture::Roughness,
                       LoadTexture(assets, modelDir,
                                   FindTexturePath(source, {{aiTextureType_DIFFUSE_ROUGHNESS, 0},
                                                            {AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE}}),
                                   ColorSpace::Linear));
            setTexture(MaterialTexture::Occlusion,
                       LoadTexture(assets, modelDir,
                                   FindTexturePath(source, {{aiTextureType_AMBIENT_OCCLUSION, 0}, {aiTextureType_LIGHTMAP, 0}}),
                                   ColorSpace::Linear));
            setTexture(MaterialTexture::Emissive,
                       LoadTexture(assets, modelDir, FindTexturePath(source, {{aiTextureType_EMISSIVE, 0}}),
                                   ColorSpace::SRGB));

            aiColor3D emissive;
            if (source.Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == aiReturn_SUCCESS) {
                material.Emissive = {emissive.r, emissive.g, emissive.b};
            } else if (material.Textures[static_cast<size_t>(MaterialTexture::Emissive)].IsValid()) {
                material.Emissive = Vec3(1.0f);
            }

            return material;
        }
    } // namespace

    bool ImportModel(AssetManager& assets, const std::filesystem::path& file, MeshData& outMesh) {
        std::string extension = file.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        unsigned flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
                         aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_SortByPType;
        // Bakes node transforms into the vertices so the mesh can be drawn without its node hierarchy.
        if (extension != ".obj") {
            flags |= aiProcess_PreTransformVertices;
        }

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(file.string(), flags);
        if (!scene || !scene->HasMeshes() || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) {
            VKE_CORE_ERROR("ModelImporter: '{}': {}", file.string(), importer.GetErrorString());
            return false;
        }

        std::filesystem::path modelDir = file.parent_path();
        outMesh.Name = file.stem().string();

        outMesh.Materials.reserve(scene->mNumMaterials);
        for (unsigned i = 0; i < scene->mNumMaterials; ++i) {
            outMesh.Materials.push_back(assets.AddMaterial(ConvertMaterial(assets, *scene->mMaterials[i], modelDir)));
        }

        bool generatedTangents = false;
        for (unsigned meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
            const aiMesh& source = *scene->mMeshes[meshIndex];
            if (!(source.mPrimitiveTypes & aiPrimitiveType_TRIANGLE) || source.mNumVertices == 0) continue;

            u32 baseVertex = static_cast<u32>(outMesh.Vertices.size());

            Submesh submesh;
            submesh.FirstIndex = static_cast<u32>(outMesh.Indices.size());
            submesh.MaterialIndex = source.mMaterialIndex;

            outMesh.Vertices.reserve(outMesh.Vertices.size() + source.mNumVertices);
            for (unsigned v = 0; v < source.mNumVertices; ++v) {
                Vertex vertex{};
                vertex.Position = ToVec3(source.mVertices[v]);
                vertex.Normal = source.HasNormals() ? ToVec3(source.mNormals[v]) : Vec3(0.0f, 1.0f, 0.0f);
                if (source.HasTextureCoords(0)) {
                    // Assimp puts V = 0 at the bottom of the image; textures are sampled with V = 0 at the top.
                    vertex.UV = {source.mTextureCoords[0][v].x, 1.0f - source.mTextureCoords[0][v].y};
                }

                if (source.HasTangentsAndBitangents()) {
                    Vec3 tangent = ToVec3(source.mTangents[v]);
                    Vec3 bitangent = ToVec3(source.mBitangents[v]);
                    f32 sign = glm::dot(glm::cross(vertex.Normal, tangent), bitangent) < 0.0f ? -1.0f : 1.0f;
                    vertex.Tangent = Vec4(tangent, sign);
                } else {
                    vertex.Tangent = FallbackTangent(vertex.Normal);
                    generatedTangents = true;
                }

                outMesh.Vertices.push_back(vertex);
            }

            outMesh.Indices.reserve(outMesh.Indices.size() + static_cast<size_t>(source.mNumFaces) * 3);
            for (unsigned f = 0; f < source.mNumFaces; ++f) {
                const aiFace& face = source.mFaces[f];
                if (face.mNumIndices != 3) continue;
                for (unsigned i = 0; i < 3; ++i) {
                    outMesh.Indices.push_back(baseVertex + face.mIndices[i]);
                }
            }

            submesh.IndexCount = static_cast<u32>(outMesh.Indices.size()) - submesh.FirstIndex;
            outMesh.Submeshes.push_back(submesh);
        }

        if (generatedTangents) {
            VKE_CORE_WARN("ModelImporter: '{}' has meshes without UVs, tangents were generated",
                          file.filename().string());
        }

        return !outMesh.Submeshes.empty();
    }

} // namespace VKE::Assets::Importers
