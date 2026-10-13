#include "EnvironmentBaker.h"
#include "ShaderLayouts.h"

#include <core/Log.h>
#include <math/MathTypes.h>
#include <platform/FileSystem.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIFramebuffer.h>

#include <algorithm>
#include <array>
#include <vector>

namespace VKE::Renderer {

    namespace {
        constexpr u32 kRadianceSize = 512;
        constexpr u32 kIrradianceSize = 32;
        constexpr u32 kPrefilterSize = 128;
        constexpr u32 kPrefilterMips = 5; // Matches kMaxReflectionLod in the shaders.
        constexpr u32 kBrdfLutSize = 512;

        constexpr RHI::Format kCubemapFormat = RHI::Format::R16G16B16A16_SFLOAT;
        constexpr RHI::Format kBrdfLutFormat = RHI::Format::R16G16_SFLOAT;
        constexpr u32 kCubeVertexCount = 36;
        constexpr u32 kFullscreenVertexCount = 3;

        // One camera per cube face, all sitting at the origin, in the order the
        // layers are stored: +X, -X, +Y, -Y, +Z, -Z.
        //
        // Each camera's up vector is the one that lands the face's texels the way
        // the cube sampler reads them back, given that the RHI draws with Y up.
        std::array<Mat4, 6> GetFaceViewProjections() {
            Mat4 projection = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

            const Vec3 origin(0.0f);
            const Vec3 up(0.0f, 1.0f, 0.0f);

            return {
                projection * glm::lookAt(origin, Vec3(1.0f, 0.0f, 0.0f), up),
                projection * glm::lookAt(origin, Vec3(-1.0f, 0.0f, 0.0f), up),
                projection * glm::lookAt(origin, Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)),
                projection * glm::lookAt(origin, Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)),
                projection * glm::lookAt(origin, Vec3(0.0f, 0.0f, 1.0f), up),
                projection * glm::lookAt(origin, Vec3(0.0f, 0.0f, -1.0f), up),
            };
        }
    } // namespace

    EnvironmentBaker::EnvironmentBaker(RHI::RHIDevice& device) : m_Device(device) {
        RHI::RenderPassDesc cubePassDesc;
        RHI::AttachmentDesc cubeFace;
        cubeFace.Format = kCubemapFormat;
        cubeFace.SampledAfter = true;
        cubePassDesc.ColorAttachments = {cubeFace};
        m_FacePass = RHI::RHIRenderPass::Create(m_Device, cubePassDesc);

        RHI::RenderPassDesc brdfPassDesc;
        RHI::AttachmentDesc brdfTarget;
        brdfTarget.Format = kBrdfLutFormat;
        brdfTarget.SampledAfter = true;
        brdfPassDesc.ColorAttachments = {brdfTarget};
        m_BrdfPass = RHI::RHIRenderPass::Create(m_Device, brdfPassDesc);

        RHI::BindGroupLayoutDesc layoutDesc;
        layoutDesc.Slots = {{0, RHI::BindingType::CombinedImageSampler, RHI::ShaderStageFlags::Fragment}};
        layoutDesc.DebugName = "EnvironmentSourceLayout";
        m_SourceLayout = RHI::RHIBindGroupLayout::Create(m_Device, layoutDesc);

        RHI::SamplerDesc samplerDesc;
        samplerDesc.AddressModeU = RHI::SamplerAddressMode::ClampToEdge;
        samplerDesc.AddressModeV = RHI::SamplerAddressMode::ClampToEdge;
        samplerDesc.AddressModeW = RHI::SamplerAddressMode::ClampToEdge;
        samplerDesc.MaxAnisotropy = 1.0f;
        samplerDesc.DebugName = "EnvironmentSourceSampler";
        m_Sampler = RHI::RHISampler::Create(m_Device, samplerDesc);

        const auto shaderDir = Platform::FileSystem::GetExecutableDirectory() / "shaders";

        // The cube is drawn from inside, so its faces point away from the camera.
        RHI::PipelineDesc cubeDesc;
        cubeDesc.VertexShaderPath = (shaderDir / "cubemap.vert.spv").string();
        cubeDesc.PushConstantRanges = {{RHI::ShaderStageFlags::AllGraphics, 0, sizeof(CapturePushConstants)}};
        cubeDesc.CullMode = RHI::CullMode::None;

        cubeDesc.FragmentShaderPath = (shaderDir / "equirect_to_cubemap.frag.spv").string();
        cubeDesc.BindGroupLayouts = {m_SourceLayout.get()};
        m_EquirectPipeline = RHI::RHIPipeline::Create(m_Device, *m_FacePass, cubeDesc);

        cubeDesc.FragmentShaderPath = (shaderDir / "irradiance.frag.spv").string();
        m_IrradiancePipeline = RHI::RHIPipeline::Create(m_Device, *m_FacePass, cubeDesc);

        cubeDesc.FragmentShaderPath = (shaderDir / "prefilter.frag.spv").string();
        m_PrefilterPipeline = RHI::RHIPipeline::Create(m_Device, *m_FacePass, cubeDesc);

        cubeDesc.FragmentShaderPath = (shaderDir / "procedural_sky.frag.spv").string();
        cubeDesc.BindGroupLayouts.clear();
        m_SkyPipeline = RHI::RHIPipeline::Create(m_Device, *m_FacePass, cubeDesc);

        RHI::PipelineDesc brdfDesc;
        brdfDesc.VertexShaderPath = (shaderDir / "fullscreen.vert.spv").string();
        brdfDesc.FragmentShaderPath = (shaderDir / "brdf_lut.frag.spv").string();
        brdfDesc.CullMode = RHI::CullMode::None;
        m_BrdfPipeline = RHI::RHIPipeline::Create(m_Device, *m_BrdfPass, brdfDesc);
    }

    Iunique<GpuEnvironment> EnvironmentBaker::Bake(RHI::RHITexture* source) {
        auto environment = MakeIunique<GpuEnvironment>();

        environment->Radiance = CreateCubemap(kRadianceSize, 0, "EnvironmentRadiance");
        Iunique<RHI::RHIBindGroup> sourceGroup = source ? CreateSourceGroup(*source) : nullptr;
        RHI::RHIPipeline& radiancePipeline = source ? *m_EquirectPipeline : *m_SkyPipeline;

        // Every mip is rendered from the source, so the prefilter pass can read
        // whichever one matches the part of the sphere a sample covers.
        for (u32 mip = 0; mip < environment->Radiance->GetMipLevels(); ++mip) {
            RenderCubeFaces(*environment->Radiance, radiancePipeline, sourceGroup.get(), mip, 0.0f);
        }

        Iunique<RHI::RHIBindGroup> radianceGroup = CreateSourceGroup(*environment->Radiance);

        environment->Irradiance = CreateCubemap(kIrradianceSize, 1, "EnvironmentIrradiance");
        RenderCubeFaces(*environment->Irradiance, *m_IrradiancePipeline, radianceGroup.get(), 0, 0.0f);

        environment->Prefilter = CreateCubemap(kPrefilterSize, kPrefilterMips, "EnvironmentPrefilter");
        for (u32 mip = 0; mip < kPrefilterMips; ++mip) {
            f32 roughness = static_cast<f32>(mip) / static_cast<f32>(kPrefilterMips - 1);
            RenderCubeFaces(*environment->Prefilter, *m_PrefilterPipeline, radianceGroup.get(), mip, roughness);
        }

        environment->BrdfLut = BakeBrdfLut();

        VKE_CORE_INFO("Renderer: baked the environment from {}", source ? "an image" : "the stand-in sky");
        return environment;
    }

    Iunique<RHI::RHITexture> EnvironmentBaker::CreateCubemap(u32 size, u32 mipLevels, const char* debugName) {
        RHI::TextureDesc desc;
        desc.Width = size;
        desc.Height = size;
        desc.ArrayLayers = 6;
        desc.MipLevels = mipLevels;
        desc.Type = RHI::TextureType::TextureCube;
        desc.Format = kCubemapFormat;
        desc.Usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
        desc.DebugName = debugName;
        return RHI::RHITexture::Create(m_Device, desc);
    }

    Iunique<RHI::RHIBindGroup> EnvironmentBaker::CreateSourceGroup(RHI::RHITexture& texture) {
        RHI::BindGroupDesc groupDesc;
        groupDesc.Layout = m_SourceLayout.get();
        groupDesc.DebugName = "EnvironmentSource";

        RHI::BoundResource resource;
        resource.Binding = 0;
        resource.Texture = &texture;
        resource.Sampler = m_Sampler.get();
        groupDesc.Resources = {resource};

        return RHI::RHIBindGroup::Create(m_Device, groupDesc);
    }

    void EnvironmentBaker::RenderCubeFaces(RHI::RHITexture& target, RHI::RHIPipeline& pipeline,
                                           RHI::RHIBindGroup* sourceGroup, u32 mipLevel, f32 roughness) {
        u32 size = std::max(target.GetWidth() >> mipLevel, 1u);

        std::vector<Iunique<RHI::RHIFramebuffer>> faceTargets;
        faceTargets.reserve(6);
        for (u32 face = 0; face < 6; ++face) {
            RHI::FramebufferDesc framebufferDesc;
            framebufferDesc.AttachmentViews = {target.GetAttachmentView(mipLevel, face)};
            framebufferDesc.Width = size;
            framebufferDesc.Height = size;
            faceTargets.push_back(RHI::RHIFramebuffer::Create(m_Device, *m_FacePass, framebufferDesc));
        }

        std::array<Mat4, 6> viewProjections = GetFaceViewProjections();

        m_Device.ExecuteImmediate([&](RHI::RHICommandBuffer& cmd) {
            for (u32 face = 0; face < 6; ++face) {
                RHI::ClearValue clear;
                cmd.BeginRenderPass(*m_FacePass, *faceTargets[face], clear);
                cmd.BindPipeline(pipeline);
                if (sourceGroup) {
                    cmd.BindBindGroup(0, *sourceGroup);
                }

                CapturePushConstants constants{};
                constants.ViewProjection = viewProjections[face];
                constants.Parameters = Vec4(roughness, 0.0f, 0.0f, 0.0f);
                cmd.PushConstants(RHI::ShaderStageFlags::AllGraphics, 0, sizeof(constants), &constants);

                cmd.Draw(kCubeVertexCount, 1, 0, 0);
                cmd.EndRenderPass();
            }
        });
    }

    Iunique<RHI::RHITexture> EnvironmentBaker::BakeBrdfLut() {
        RHI::TextureDesc desc;
        desc.Width = kBrdfLutSize;
        desc.Height = kBrdfLutSize;
        desc.Format = kBrdfLutFormat;
        desc.Usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
        desc.DebugName = "EnvironmentBrdfLut";
        Iunique<RHI::RHITexture> brdfLut = RHI::RHITexture::Create(m_Device, desc);

        RHI::FramebufferDesc framebufferDesc;
        framebufferDesc.AttachmentViews = {brdfLut->GetAttachmentView(0, 0)};
        framebufferDesc.Width = kBrdfLutSize;
        framebufferDesc.Height = kBrdfLutSize;
        Iunique<RHI::RHIFramebuffer> target = RHI::RHIFramebuffer::Create(m_Device, *m_BrdfPass, framebufferDesc);

        m_Device.ExecuteImmediate([&](RHI::RHICommandBuffer& cmd) {
            RHI::ClearValue clear;
            cmd.BeginRenderPass(*m_BrdfPass, *target, clear);
            cmd.BindPipeline(*m_BrdfPipeline);
            cmd.Draw(kFullscreenVertexCount, 1, 0, 0);
            cmd.EndRenderPass();
        });

        return brdfLut;
    }

} // namespace VKE::Renderer
