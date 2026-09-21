#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>

namespace VKE::RHI {

    class RHIDevice;

    // GPU image, used as a render target or sampled in shaders.
    class RHITexture {
    public:
        virtual ~RHITexture() = default;

        // `initialData` holds the pixels of the base level, tightly packed in desc.Format.
        static Iunique<RHITexture> Create(RHIDevice& device, const TextureDesc& desc, const void* initialData = nullptr);

        virtual u32 GetWidth() const = 0;
        virtual u32 GetHeight() const = 0;
        virtual Format GetFormat() const = 0;
        virtual u32 GetMipLevels() const = 0;
        virtual u32 GetArrayLayers() const = 0;

        // Opaque view of the whole texture, used to sample it.
        virtual void* GetView() const = 0;

        // Opaque view of a single mip level of a single layer, used as an
        // RHIFramebuffer attachment to render into that part of the texture.
        virtual void* GetAttachmentView(u32 mipLevel, u32 layer) = 0;
    };

} // namespace VKE::RHI
