#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>

namespace VKE::RHI {

    class RHIDevice;

    // Describes a set of attachments and how a single subpass reads/writes
    // them. Multi-subpass render passes aren't supported yet — RenderPassDesc
    // can grow subpass support later without breaking this interface.
    class RHIRenderPass {
    public:
        virtual ~RHIRenderPass() = default;

        static Iunique<RHIRenderPass> Create(RHIDevice& device, const RenderPassDesc& desc);
    };

} // namespace VKE::RHI
