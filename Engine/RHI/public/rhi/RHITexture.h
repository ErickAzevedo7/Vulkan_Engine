#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>

// Not implemented yet — nothing currently uploads a texture. Calling this
// logs a warning and returns nullptr, the same shape as
// Platform::FileDialog's stub.

namespace VKE::RHI {

    class RHIDevice;

    class RHITexture {
    public:
        virtual ~RHITexture() = default;

        static Iunique<RHITexture> Create(RHIDevice& device, const TextureDesc& desc, const void* initialData = nullptr);
    };

} // namespace VKE::RHI
