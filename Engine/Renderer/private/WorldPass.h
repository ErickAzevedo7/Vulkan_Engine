#pragma once

#include <core/Types.h>
#include <math/MathTypes.h>
#include <renderer/WorldDraw.h>
#include <rhi/RHIBindGroup.h>
#include <rhi/RHIBuffer.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIPipeline.h>
#include <rhi/RHIRenderPass.h>
#include <rhi/RHISampler.h>
#include <rhi/RHITexture.h>
#include <functional>
#include <unordered_map>
#include <vector>

namespace VKE::Renderer {

    // Draws world draw lists over a scene's displayable color, tested against the scene's depth
    // or on top of it.
    class WorldPass {
    public:
        // `renderPass` is the pass Record() is called in: displayable color and the scene's
        // depth, both kept.
        WorldPass(RHI::RHIDevice& device, RHI::RHIRenderPass& renderPass, u32 framesInFlight);
        ~WorldPass();

        // The texture a command names, or null to draw it untextured.
        using TextureResolver = std::function<RHI::RHITexture*(const OverlayTexture&)>;

        // Frees the frame's room for vertices, once the GPU is done with the frame that last used it.
        void BeginFrame(u32 frameIndex);

        // Can be called several times a frame, for different views.
        void Record(RHI::RHICommandBuffer& cmd, u32 frameIndex, const WorldDrawList& list, const Mat4& viewProjection,
                    const Vec3& cameraPosition, u32 width, u32 height, const TextureResolver& resolve);

        // Drops what the pass keeps for `texture`. Call before destroying a texture it drew,
        // once the GPU is done with it.
        void ForgetTexture(RHI::RHITexture& texture);

    private:
        // Room for the lists recorded in a frame. A frame that needs more adds a block, and the
        // next frame in the same slot merges them into one.
        struct Block {
            Iunique<RHI::RHIBuffer> Vertices;
            Iunique<RHI::RHIBuffer> Indices;
            size_t VerticesUsed = 0;
            size_t IndicesUsed = 0;
        };

        RHI::RHIBindGroup& GetBindGroup(RHI::RHITexture& texture);
        Block MakeBlock(size_t vertexBytes, size_t indexBytes);

        RHI::RHIDevice& m_Device;

        Iunique<RHI::RHIBindGroupLayout> m_Layout;
        Iunique<RHI::RHISampler> m_Sampler;
        Iunique<RHI::RHIPipeline> m_TestedPipeline;
        Iunique<RHI::RHIPipeline> m_OnTopPipeline;
        // Sampled by untextured commands.
        Iunique<RHI::RHITexture> m_White;

        // One list of blocks per frame in flight.
        std::vector<std::vector<Block>> m_Blocks;
        std::unordered_map<RHI::RHITexture*, Iunique<RHI::RHIBindGroup>> m_BindGroups;
    };

} // namespace VKE::Renderer
