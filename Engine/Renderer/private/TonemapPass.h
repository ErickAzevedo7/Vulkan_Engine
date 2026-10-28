#pragma once

#include <core/Types.h>
#include <rhi/RHIBindGroup.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIPipeline.h>
#include <rhi/RHIRenderPass.h>
#include <rhi/RHISampler.h>
#include <rhi/RHITexture.h>
#include <unordered_map>

namespace VKE::Renderer {

    // Draws a high dynamic range texture over the whole output of a render pass, turning it
    // into displayable color.
    class TonemapPass {
    public:
        explicit TonemapPass(RHI::RHIDevice& device);

        // The bind group Record() reads `source` through. Make a new one whenever the
        // texture is recreated.
        Iunique<RHI::RHIBindGroup> CreateSource(RHI::RHITexture& source);

        // Must be called inside `outputPass`. `encodeSrgb` writes sRGB-encoded values, for
        // outputs whose format does not encode by itself (UNORM); otherwise the result stays linear.
        void Record(RHI::RHICommandBuffer& cmd, RHI::RHIRenderPass& outputPass, RHI::RHIBindGroup& source,
                    f32 exposure, bool encodeSrgb);

    private:
        RHI::RHIPipeline& GetPipeline(RHI::RHIRenderPass& outputPass);

        RHI::RHIDevice& m_Device;

        Iunique<RHI::RHIBindGroupLayout> m_Layout;
        Iunique<RHI::RHISampler> m_Sampler;
        // One per output pass it has drawn in; the passes must outlive this.
        std::unordered_map<const RHI::RHIRenderPass*, Iunique<RHI::RHIPipeline>> m_Pipelines;
    };

} // namespace VKE::Renderer
