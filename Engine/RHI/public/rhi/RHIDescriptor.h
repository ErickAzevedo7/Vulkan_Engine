#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>

// Not implemented yet — see RHITexture.h's note. No pipeline currently uses
// a descriptor set, so there is nothing yet to bind through this interface.
// Real binding-layout design (uniform buffers, combined image samplers, ...)
// is unwritten.

namespace VKE::RHI {

    class RHIDevice;

    class RHIDescriptorSet {
    public:
        virtual ~RHIDescriptorSet() = default;

        static Iunique<RHIDescriptorSet> Create(RHIDevice& device, const DescriptorSetLayoutDesc& desc);
    };

} // namespace VKE::RHI
