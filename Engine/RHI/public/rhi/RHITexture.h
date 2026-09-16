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

        // Opaque view, used as an RHIFramebuffer attachment.
        virtual void* GetView() const = 0;
    };

} // namespace VKE::RHI
