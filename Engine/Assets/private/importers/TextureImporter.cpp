#include "Importers.h"

#include <core/Log.h>
#include <platform/FileSystem.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace VKE::Assets::Importers {

    bool ImportTexture(const std::filesystem::path& file, ColorSpace colorSpace, TextureData& outTexture) {
        std::vector<Byte> bytes = Platform::FileSystem::ReadFileBytes(file.string());
        if (bytes.empty()) return false;

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels,
                                                STBI_rgb_alpha);
        if (!pixels) {
            VKE_CORE_ERROR("TextureImporter: '{}': {}", file.string(), stbi_failure_reason());
            return false;
        }

        outTexture.Name = file.filename().string();
        outTexture.Width = static_cast<u32>(width);
        outTexture.Height = static_cast<u32>(height);
        outTexture.ColorSpace = colorSpace;
        outTexture.Pixels.assign(pixels, pixels + static_cast<size_t>(width) * static_cast<size_t>(height) * 4);

        stbi_image_free(pixels);
        return true;
    }

} // namespace VKE::Assets::Importers
