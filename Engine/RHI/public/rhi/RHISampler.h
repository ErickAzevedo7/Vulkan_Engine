#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>

// Not implemented yet — see RHITexture.h's note; a sampler has no purpose
// without a texture to sample.

namespace VKE::RHI {

    class RHIDevice;

    class RHISampler {
    public:
        virtual ~RHISampler() = default;

        static Iunique<RHISampler> Create(RHIDevice& device, const SamplerDesc& desc);
    };

} // namespace VKE::RHI
