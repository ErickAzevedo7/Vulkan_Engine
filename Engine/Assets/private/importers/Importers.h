#pragma once

#include <assets/AssetTypes.h>
#include <filesystem>

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Assets::Importers {

    // Reads the node hierarchy, geometry and materials of a model file (glTF, OBJ, FBX, ...).
    // Meshes and materials are added to `assets`.
    bool ImportModel(AssetManager& assets, const std::filesystem::path& file, ModelData& outModel);

    // Decodes an image file into RGBA8 pixels.
    bool ImportTexture(const std::filesystem::path& file, ColorSpace colorSpace, TextureData& outTexture);

    // Reads a .mat YAML file and loads the textures it references.
    bool ImportMaterial(AssetManager& assets, const std::filesystem::path& file, MaterialData& outMaterial);

} // namespace VKE::Assets::Importers
