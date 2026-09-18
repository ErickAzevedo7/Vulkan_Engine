#include "Importers.h"

#include <assets/AssetManager.h>
#include <core/Log.h>
#include <platform/FileSystem.h>

#include <nlohmann/json.hpp>

namespace VKE::Assets::Importers {

    namespace {
        struct TextureKey {
            const char* Key;
            MaterialTexture Slot;
            ColorSpace Space;
        };

        const TextureKey kTextureKeys[] = {
            {"albedoTextureKey", MaterialTexture::Albedo, ColorSpace::SRGB},
            {"normalTextureKey", MaterialTexture::Normal, ColorSpace::Linear},
            {"metallicTextureKey", MaterialTexture::Metallic, ColorSpace::Linear},
            {"roughnessTextureKey", MaterialTexture::Roughness, ColorSpace::Linear},
            {"aoTextureKey", MaterialTexture::Occlusion, ColorSpace::Linear},
            {"emissiveTextureKey", MaterialTexture::Emissive, ColorSpace::SRGB},
        };

        f32 ReadFloat(const nlohmann::json& json, const char* key, f32 fallback) {
            auto it = json.find(key);
            return it != json.end() && it->is_number() ? it->get<f32>() : fallback;
        }

        Vec3 ReadVec3(const nlohmann::json& json, const char* key, const Vec3& fallback) {
            auto it = json.find(key);
            if (it == json.end() || !it->is_array() || it->size() != 3) return fallback;
            for (const auto& component : *it) {
                if (!component.is_number()) return fallback;
            }
            return {(*it)[0].get<f32>(), (*it)[1].get<f32>(), (*it)[2].get<f32>()};
        }

        std::string ReadString(const nlohmann::json& json, const char* key) {
            auto it = json.find(key);
            return it != json.end() && it->is_string() ? it->get<std::string>() : std::string();
        }
    } // namespace

    bool ImportMaterial(AssetManager& assets, const std::filesystem::path& file, MaterialData& outMaterial) {
        std::string text = Platform::FileSystem::ReadFileText(file.string());
        if (text.empty()) return false;

        nlohmann::json json = nlohmann::json::parse(text, nullptr, false);
        if (json.is_discarded() || !json.is_object()) {
            VKE_CORE_ERROR("MaterialImporter: '{}' is not a valid material file", file.string());
            return false;
        }

        std::string name = ReadString(json, "name");
        outMaterial.Name = name.empty() ? file.stem().string() : name;
        outMaterial.BaseColor = Vec4(ReadVec3(json, "albedo", Vec3(1.0f)), 1.0f);
        outMaterial.Metallic = ReadFloat(json, "metallic", 1.0f);
        outMaterial.Roughness = ReadFloat(json, "roughness", 1.0f);
        outMaterial.Occlusion = ReadFloat(json, "ao", 1.0f);

        for (const TextureKey& entry : kTextureKeys) {
            std::string path = ReadString(json, entry.Key);
            if (path.empty()) continue;
            outMaterial.Textures[static_cast<size_t>(entry.Slot)] = assets.LoadTexture(path, entry.Space);
        }

        bool hasEmissiveTexture = outMaterial.Textures[static_cast<size_t>(MaterialTexture::Emissive)].IsValid();
        outMaterial.Emissive = ReadVec3(json, "emissive", Vec3(hasEmissiveTexture ? 1.0f : 0.0f));

        return true;
    }

} // namespace VKE::Assets::Importers
