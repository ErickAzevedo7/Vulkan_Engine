#pragma once

#include <assets/AssetTypes.h>
#include <filesystem>

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Assets::Importers {

    // Reads geometry and materials from a model file (glTF, OBJ, FBX, ...).
    bool ImportModel(AssetManager& assets, const std::filesystem::path& file, MeshData& outMesh);

    // Decodes an image file into RGBA8 pixels.
    bool ImportTexture(const std::filesystem::path& file, ColorSpace colorSpace, TextureData& outTexture);

    // Reads a .mat JSON file and loads the textures it references.
    bool ImportMaterial(AssetManager& assets, const std::filesystem::path& file, MaterialData& outMaterial);

} // namespace VKE::Assets::Importers
