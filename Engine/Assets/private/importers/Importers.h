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
    // Writes a material as a .mat file, its textures by asset ID. Textures without one, like the
    // default ones, are left out. The name isn't written: it is the file's.
    bool ExportMaterial(const AssetManager& assets, const MaterialData& material, const std::filesystem::path& file);

} // namespace VKE::Assets::Importers
