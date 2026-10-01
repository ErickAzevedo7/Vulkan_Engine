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

        // .hdr images hold values above 1 and are always linear.
        bool isFloat = stbi_is_hdr_from_memory(bytes.data(), static_cast<int>(bytes.size())) != 0;

        void* pixels = nullptr;
        size_t bytesPerPixel = 4;
        if (isFloat) {
            pixels = stbi_loadf_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels,
                                            STBI_rgb_alpha);
            bytesPerPixel = 4 * sizeof(float);
        } else {
            pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels,
                                           STBI_rgb_alpha);
        }

        if (!pixels) {
            VKE_CORE_ERROR("TextureImporter: '{}': {}", file.string(), stbi_failure_reason());
            return false;
        }

        outTexture.Name = file.filename().string();
        outTexture.Width = static_cast<u32>(width);
        outTexture.Height = static_cast<u32>(height);
        outTexture.ColorSpace = isFloat ? ColorSpace::Linear : colorSpace;
        outTexture.PixelType = isFloat ? PixelType::Float32 : PixelType::UNorm8;
        const u8* first = static_cast<const u8*>(pixels);
        outTexture.Pixels.assign(first, first + static_cast<size_t>(width) * height * bytesPerPixel);

        stbi_image_free(pixels);
        return true;
    }

} // namespace VKE::Assets::Importers
