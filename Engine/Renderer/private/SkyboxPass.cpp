#include "SkyboxPass.h"
#include "ShaderLayouts.h"

#include <platform/FileSystem.h>

namespace VKE::Renderer {

    namespace {
        constexpr u32 kCubeVertexCount = 36;
    } // namespace

    SkyboxPass::SkyboxPass(RHI::RHIDevice& device, RHI::RHIRenderPass& scenePass, RHI::RHIBindGroupLayout& frameLayout,
                           RHI::RHIBindGroupLayout& environmentLayout) {
        const auto shaderDir = Platform::FileSystem::GetExecutableDirectory() / "shaders";

        RHI::PipelineDesc desc;
        desc.VertexShaderPath = (shaderDir / "skybox.vert.spv").string();
        desc.FragmentShaderPath = (shaderDir / "skybox.frag.spv").string();
        desc.BindGroupLayouts = {&frameLayout, &environmentLayout};
        desc.CullMode = RHI::CullMode::None;
        // Drawn after the scene, at the far plane, so it only fills empty pixels.
        desc.DepthTestEnable = true;
        desc.DepthWriteEnable = false;
        desc.DepthCompareOp = RHI::CompareOp::LessOrEqual;
        m_Pipeline = RHI::RHIPipeline::Create(device, scenePass, desc);
    }

    void SkyboxPass::Record(RHI::RHICommandBuffer& cmd, RHI::RHIBindGroup& frameGroup,
                            RHI::RHIBindGroup& environmentGroup) {
        cmd.BindPipeline(*m_Pipeline);
        cmd.BindBindGroup(kFrameSet, frameGroup);
        cmd.BindBindGroup(kEnvironmentSet, environmentGroup);
        cmd.Draw(kCubeVertexCount, 1, 0, 0);
    }

} // namespace VKE::Renderer
