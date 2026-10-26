#include "Importers.h"
#include "YamlUtils.h"

#include <assets/AssetManager.h>
#include <core/Log.h>

namespace VKE::Assets::Importers {

    namespace {
        struct TextureSlot {
            const char* Key;
            MaterialTexture Slot;
            ColorSpace Space;
        };

        const TextureSlot kTextureSlots[] = {
            {"Albedo", MaterialTexture::Albedo, ColorSpace::SRGB},
            {"Normal", MaterialTexture::Normal, ColorSpace::Linear},
            {"Metallic", MaterialTexture::Metallic, ColorSpace::Linear},
            {"Roughness", MaterialTexture::Roughness, ColorSpace::Linear},
            {"Occlusion", MaterialTexture::Occlusion, ColorSpace::Linear},
            {"Emissive", MaterialTexture::Emissive, ColorSpace::SRGB},
        };
    } // namespace

    bool ImportMaterial(AssetManager& assets, const std::filesystem::path& file, MaterialData& outMaterial) {
        YAML::Node root;
        try {
            root = YAML::LoadFile(file.string());
        } catch (const YAML::Exception& error) {
            VKE_CORE_ERROR("MaterialImporter: '{}': {}", file.string(), error.what());
            return false;
        }
        if (!root.IsMap()) {
            VKE_CORE_ERROR("MaterialImporter: '{}' is not a material file", file.string());
            return false;
        }

        outMaterial.Name = ReadYaml<std::string>(root, "Name", file.stem().string());
        outMaterial.BaseColor = ReadYamlVector<Vec4, 4>(root, "BaseColor", Vec4(1.0f));
        outMaterial.Metallic = ReadYaml<f32>(root, "Metallic", 1.0f);
        outMaterial.Roughness = ReadYaml<f32>(root, "Roughness", 1.0f);
        outMaterial.Occlusion = ReadYaml<f32>(root, "Occlusion", 1.0f);

        // Textures by asset ID.
        YAML::Node textures = root["Textures"];
        if (textures && textures.IsMap()) {
            for (const TextureSlot& entry : kTextureSlots) {
                std::string id = ReadYaml<std::string>(textures, entry.Key, std::string());
                if (id.empty()) continue;
                outMaterial.Textures[static_cast<size_t>(entry.Slot)] = assets.LoadTexture(AssetID::Parse(id), entry.Space);
            }
        }

        bool hasEmissiveTexture = outMaterial.Textures[static_cast<size_t>(MaterialTexture::Emissive)].IsValid();
        outMaterial.Emissive = ReadYamlVector<Vec3, 3>(root, "Emissive", Vec3(hasEmissiveTexture ? 1.0f : 0.0f));
        return true;
    }

} // namespace VKE::Assets::Importers
