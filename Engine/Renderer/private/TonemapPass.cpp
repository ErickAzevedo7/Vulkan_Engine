#include "TonemapPass.h"

#include <core/Assert.h>
#include <platform/FileSystem.h>

namespace VKE::Renderer {

    TonemapPass::TonemapPass(RHI::RHIDevice& device, RHI::RHIRenderPass& outputPass) : m_Device(device) {
        RHI::BindGroupLayoutDesc layoutDesc;
        layoutDesc.Slots = {{0, RHI::BindingType::CombinedImageSampler, RHI::ShaderStageFlags::Fragment}};
        layoutDesc.DebugName = "TonemapLayout";
        m_Layout = RHI::RHIBindGroupLayout::Create(m_Device, layoutDesc);

        RHI::SamplerDesc samplerDesc;
        samplerDesc.AddressModeU = RHI::SamplerAddressMode::ClampToEdge;
        samplerDesc.AddressModeV = RHI::SamplerAddressMode::ClampToEdge;
        samplerDesc.MaxAnisotropy = 1.0f;
        samplerDesc.DebugName = "TonemapSampler";
        m_Sampler = RHI::RHISampler::Create(m_Device, samplerDesc);

        const auto shaderDir = Platform::FileSystem::GetExecutableDirectory() / "shaders";

        // The vertex shader builds a full-screen triangle, so there is no vertex input.
        RHI::PipelineDesc pipelineDesc;
        pipelineDesc.VertexShaderPath = (shaderDir / "fullscreen.vert.spv").string();
        pipelineDesc.FragmentShaderPath = (shaderDir / "tonemap.frag.spv").string();
        pipelineDesc.BindGroupLayouts = {m_Layout.get()};
        pipelineDesc.PushConstantRanges = {{RHI::ShaderStageFlags::Fragment, 0, sizeof(f32)}};
        pipelineDesc.CullMode = RHI::CullMode::None;
        m_Pipeline = RHI::RHIPipeline::Create(m_Device, outputPass, pipelineDesc);
    }

    void TonemapPass::SetSources(const std::vector<RHI::RHITexture*>& sources) {
        m_BindGroups.clear();
        m_BindGroups.reserve(sources.size());

        for (RHI::RHITexture* source : sources) {
            RHI::BindGroupDesc groupDesc;
            groupDesc.Layout = m_Layout.get();
            groupDesc.DebugName = "TonemapSource";

            RHI::BoundResource resource;
            resource.Binding = 0;
            resource.Texture = source;
            resource.Sampler = m_Sampler.get();
            groupDesc.Resources = {resource};

            m_BindGroups.push_back(RHI::RHIBindGroup::Create(m_Device, groupDesc));
        }
    }

    void TonemapPass::Record(RHI::RHICommandBuffer& cmd, u32 frameIndex, f32 exposure) {
        VKE_CORE_ASSERT(frameIndex < m_BindGroups.size(), "Tonemap pass has no source for this frame");

        cmd.BindPipeline(*m_Pipeline);
        cmd.BindBindGroup(0, *m_BindGroups[frameIndex]);
        cmd.PushConstants(RHI::ShaderStageFlags::Fragment, 0, sizeof(exposure), &exposure);
        cmd.Draw(3, 1, 0, 0);
    }

} // namespace VKE::Renderer
