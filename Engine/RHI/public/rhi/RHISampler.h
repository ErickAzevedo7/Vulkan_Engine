#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>

namespace VKE::RHI {

    class RHIDevice;

    // Filtering and addressing state used to sample textures.
    class RHISampler {
    public:
        virtual ~RHISampler() = default;

        static Iunique<RHISampler> Create(RHIDevice& device, const SamplerDesc& desc);
    };

} // namespace VKE::RHI
