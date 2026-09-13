#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>

namespace VKE::RHI {

    class RHIDevice;
    class RHIRenderPass;

    // A compiled graphics pipeline, built from a generic PipelineDesc.
    // Compiled against a specific RHIRenderPass; only usable with a render
    // pass its attachments are compatible with.
    class RHIPipeline {
    public:
        virtual ~RHIPipeline() = default;

        static Iunique<RHIPipeline> Create(RHIDevice& device, RHIRenderPass& renderPass, const PipelineDesc& desc);
    };

} // namespace VKE::RHI
