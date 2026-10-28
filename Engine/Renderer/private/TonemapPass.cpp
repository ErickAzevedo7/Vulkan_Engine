#include "TonemapPass.h"

#include <platform/FileSystem.h>

namespace VKE::Renderer {

    namespace {
        // Must match TonemapConstants in shaders/tonemap.frag.
        struct TonemapConstants {
            f32 Exposure;
            u32 EncodeSrgb;
        };
    } // namespace

    TonemapPass::TonemapPass(RHI::RHIDevice& device) : m_Device(device) {
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
    }

    RHI::RHIPipeline& TonemapPass::GetPipeline(RHI::RHIRenderPass& outputPass) {
        Iunique<RHI::RHIPipeline>& pipeline = m_Pipelines[&outputPass];
        if (pipeline) return *pipeline;

        const auto shaderDir = Platform::FileSystem::GetExecutableDirectory() / "shaders";

        // The vertex shader builds a full-screen triangle, so there is no vertex input.
        RHI::PipelineDesc pipelineDesc;
        pipelineDesc.VertexShaderPath = (shaderDir / "fullscreen.vert.spv").string();
        pipelineDesc.FragmentShaderPath = (shaderDir / "tonemap.frag.spv").string();
        pipelineDesc.BindGroupLayouts = {m_Layout.get()};
        pipelineDesc.PushConstantRanges = {{RHI::ShaderStageFlags::Fragment, 0, sizeof(TonemapConstants)}};
        pipelineDesc.CullMode = RHI::CullMode::None;
        pipeline = RHI::RHIPipeline::Create(m_Device, outputPass, pipelineDesc);
        return *pipeline;
    }

    Iunique<RHI::RHIBindGroup> TonemapPass::CreateSource(RHI::RHITexture& source) {
        RHI::BindGroupDesc groupDesc;
        groupDesc.Layout = m_Layout.get();
        groupDesc.DebugName = "TonemapSource";

        RHI::BoundResource resource;
        resource.Binding = 0;
        resource.Texture = &source;
        resource.Sampler = m_Sampler.get();
        groupDesc.Resources = {resource};

        return RHI::RHIBindGroup::Create(m_Device, groupDesc);
    }

    void TonemapPass::Record(RHI::RHICommandBuffer& cmd, RHI::RHIRenderPass& outputPass, RHI::RHIBindGroup& source,
                             f32 exposure, bool encodeSrgb) {
        TonemapConstants constants{exposure, encodeSrgb ? 1u : 0u};
        cmd.BindPipeline(GetPipeline(outputPass));
        cmd.BindBindGroup(0, source);
        cmd.PushConstants(RHI::ShaderStageFlags::Fragment, 0, sizeof(constants), &constants);
        cmd.Draw(3, 1, 0, 0);
    }

} // namespace VKE::Renderer
