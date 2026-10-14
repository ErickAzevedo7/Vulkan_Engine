#include "ForwardPath.h"
#include "ShaderLayouts.h"

#include <assets/AssetManager.h>
#include <core/Log.h>
#include <platform/FileSystem.h>
#include <platform/Window.h>
#include <renderer/RenderScene.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <functional>

namespace VKE::Renderer {

    namespace {
        static_assert(sizeof(Assets::Vertex) == 48, "Vertex layout changed, update the pipeline attributes");

        // Keeps light values above 1.0 instead of clipping them at the swapchain.
        constexpr RHI::Format kSceneColorFormat = RHI::Format::R16G16B16A16_SFLOAT;

        // Box around the eight transformed corners of `bounds`.
        AABB TransformBounds(const AABB& bounds, const Mat4& transform) {
            AABB result(Vec3(FLT_MAX), Vec3(-FLT_MAX));
            for (u32 corner = 0; corner < 8; ++corner) {
                Vec3 point((corner & 1) ? bounds.Max.x : bounds.Min.x, (corner & 2) ? bounds.Max.y : bounds.Min.y,
                           (corner & 4) ? bounds.Max.z : bounds.Min.z);
                Vec3 world = Vec3(transform * Vec4(point, 1.0f));
                result.Min = glm::min(result.Min, world);
                result.Max = glm::max(result.Max, world);
            }
            return result;
        }

        Vec3 SafeNormalize(const Vec3& v) {
            f32 length = glm::length(v);
            return length > 0.0f ? v / length : Vec3(0.0f);
        }
    } // namespace

    ForwardPath::ForwardPath(Platform::IWindow& window, Assets::AssetManager& assets) : m_Window(window) {
        m_Device = RHI::RHIDevice::Create(m_Window);
        m_Swapchain = RHI::RHISwapchain::Create(*m_Device, m_Window);

        RHI::RenderPassDesc scenePassDesc;
        RHI::AttachmentDesc sceneColor;
        sceneColor.Format = kSceneColorFormat;
        sceneColor.SampledAfter = true;
        scenePassDesc.ColorAttachments = {sceneColor};
        scenePassDesc.HasDepthAttachment = true;
        scenePassDesc.DepthAttachment.Format = m_Device->GetDepthFormat();
        scenePassDesc.DepthAttachment.IsDepth = true;
        m_ScenePass = RHI::RHIRenderPass::Create(*m_Device, scenePassDesc);

        RHI::RenderPassDesc outputPassDesc;
        RHI::AttachmentDesc outputColor;
        outputColor.Format = m_Swapchain->GetColorFormat();
        outputColor.PresentAfter = true;
        outputPassDesc.ColorAttachments = {outputColor};
        m_OutputPass = RHI::RHIRenderPass::Create(*m_Device, outputPassDesc);

        RHI::BindGroupLayoutDesc frameLayoutDesc;
        frameLayoutDesc.Slots = {{0, RHI::BindingType::UniformBuffer, RHI::ShaderStageFlags::AllGraphics}};
        frameLayoutDesc.DebugName = "FrameLayout";
        m_FrameLayout = RHI::RHIBindGroupLayout::Create(*m_Device, frameLayoutDesc);

        RHI::BindGroupLayoutDesc environmentLayoutDesc;
        for (u32 binding :
             {kIrradianceBinding, kPrefilterBinding, kBrdfLutBinding, kRadianceBinding, kShadowMapBinding,
              kPointShadowMapBinding}) {
            environmentLayoutDesc.Slots.push_back(
                {binding, RHI::BindingType::CombinedImageSampler, RHI::ShaderStageFlags::Fragment});
        }
        environmentLayoutDesc.DebugName = "EnvironmentLayout";
        m_EnvironmentLayout = RHI::RHIBindGroupLayout::Create(*m_Device, environmentLayoutDesc);

        RHI::BindGroupLayoutDesc materialLayoutDesc;
        for (u32 slot = 0; slot < static_cast<u32>(Assets::MaterialTexture::Count); ++slot) {
            materialLayoutDesc.Slots.push_back(
                {slot, RHI::BindingType::CombinedImageSampler, RHI::ShaderStageFlags::Fragment});
        }
        materialLayoutDesc.Slots.push_back(
            {kMaterialUniformsBinding, RHI::BindingType::UniformBuffer, RHI::ShaderStageFlags::Fragment});
        materialLayoutDesc.DebugName = "MaterialLayout";
        m_MaterialLayout = RHI::RHIBindGroupLayout::Create(*m_Device, materialLayoutDesc);

        CreatePipeline();

        m_GpuCache = MakeIunique<GpuCache>(*m_Device, assets, *m_MaterialLayout);

        RHI::SamplerDesc environmentSamplerDesc;
        environmentSamplerDesc.AddressModeU = RHI::SamplerAddressMode::ClampToEdge;
        environmentSamplerDesc.AddressModeV = RHI::SamplerAddressMode::ClampToEdge;
        environmentSamplerDesc.AddressModeW = RHI::SamplerAddressMode::ClampToEdge;
        environmentSamplerDesc.MaxAnisotropy = 1.0f;
        environmentSamplerDesc.DebugName = "EnvironmentSampler";
        m_EnvironmentSampler = RHI::RHISampler::Create(*m_Device, environmentSamplerDesc);

        m_ShadowPass = MakeIunique<ShadowPass>(*m_Device);
        m_SkyboxPass = MakeIunique<SkyboxPass>(*m_Device, *m_ScenePass, *m_FrameLayout, *m_EnvironmentLayout);
        m_TonemapPass = MakeIunique<TonemapPass>(*m_Device, *m_OutputPass);

        CreateTargets();

        for (u32 i = 0; i < m_Swapchain->GetFramesInFlightCount(); ++i) {
            RHI::BufferDesc uniformDesc;
            uniformDesc.Size = sizeof(FrameUniforms);
            uniformDesc.Usage = RHI::BufferUsage::Uniform;
            uniformDesc.Memory = RHI::BufferMemory::HostVisible;
            uniformDesc.DebugName = "FrameUniforms";
            m_FrameUniformBuffers.push_back(RHI::RHIBuffer::Create(*m_Device, uniformDesc));

            RHI::BindGroupDesc groupDesc;
            groupDesc.Layout = m_FrameLayout.get();
            groupDesc.Resources = {{0, m_FrameUniformBuffers.back().get()}};
            groupDesc.DebugName = "FrameGroup";
            m_FrameGroups.push_back(RHI::RHIBindGroup::Create(*m_Device, groupDesc));

            m_CommandBuffers.push_back(RHI::RHICommandBuffer::Create(*m_Device));
        }
    }

    ForwardPath::~ForwardPath() = default;

    void ForwardPath::WaitIdle() {
        m_Device->WaitIdle();
    }

    void ForwardPath::CreatePipeline() {
        const auto shaderDir = Platform::FileSystem::GetExecutableDirectory() / "shaders";

        RHI::PipelineDesc desc;
        desc.VertexShaderPath = (shaderDir / "forward.vert.spv").string();
        desc.FragmentShaderPath = (shaderDir / "forward.frag.spv").string();
        desc.VertexStride = sizeof(Assets::Vertex);
        desc.VertexAttributes = {
            {0, RHI::Format::R32G32B32_SFLOAT, offsetof(Assets::Vertex, Position)},
            {1, RHI::Format::R32G32B32_SFLOAT, offsetof(Assets::Vertex, Normal)},
            {2, RHI::Format::R32G32_SFLOAT, offsetof(Assets::Vertex, UV)},
            {3, RHI::Format::R32G32B32A32_SFLOAT, offsetof(Assets::Vertex, Tangent)},
        };
        desc.BindGroupLayouts = {m_FrameLayout.get(), m_EnvironmentLayout.get(), m_MaterialLayout.get()};
        desc.PushConstantRanges = {{RHI::ShaderStageFlags::Vertex, 0, sizeof(ObjectPushConstants)}};
        desc.CullMode = RHI::CullMode::Back;
        desc.FrontFace = RHI::FrontFace::CounterClockwise;
        desc.DepthTestEnable = true;
        desc.DepthWriteEnable = true;
        m_Pipeline = RHI::RHIPipeline::Create(*m_Device, *m_ScenePass, desc);
    }

    void ForwardPath::CreateTargets() {
        m_OutputFramebuffers.clear();
        m_SceneTargets.clear();

        u32 width = m_Swapchain->GetWidth();
        u32 height = m_Swapchain->GetHeight();

        // One scene target per frame in flight, so a frame never draws over the
        // target the previous one is still presenting.
        m_SceneTargets.resize(m_Swapchain->GetFramesInFlightCount());
        std::vector<RHI::RHITexture*> sources;
        sources.reserve(m_SceneTargets.size());
        for (RenderTarget& target : m_SceneTargets) {
            target.Create(*m_Device, *m_ScenePass, width, height, kSceneColorFormat, m_Device->GetDepthFormat(),
                          "SceneTarget");
            sources.push_back(target.Color.get());
        }
        m_TonemapPass->SetSources(sources);

        for (u32 i = 0; i < m_Swapchain->GetImageCount(); ++i) {
            RHI::FramebufferDesc framebufferDesc;
            framebufferDesc.AttachmentViews = {m_Swapchain->GetImageView(i)};
            framebufferDesc.Width = width;
            framebufferDesc.Height = height;
            m_OutputFramebuffers.push_back(RHI::RHIFramebuffer::Create(*m_Device, *m_OutputPass, framebufferDesc));
        }
    }

    void ForwardPath::RecreateSwapchain() {
        // Framebuffers reference the swapchain views, so release them before recreating.
        m_Device->WaitIdle();
        m_OutputFramebuffers.clear();
        m_SceneTargets.clear();
        m_Swapchain->Recreate();
        CreateTargets();
    }

    void ForwardPath::CollectDrawItems(const RenderScene& scene) {
        m_DrawItems.clear();
        m_SceneBounds = AABB(Vec3(FLT_MAX), Vec3(-FLT_MAX));

        for (const MeshInstanceDesc& instance : scene.GetMeshes().Values()) {
            if (!instance.Visible) continue;

            const GpuMesh* mesh = m_GpuCache->GetMesh(instance.Mesh);
            if (!mesh) continue;

            for (size_t i = 0; i < mesh->Submeshes.size(); ++i) {
                const Assets::Submesh& submesh = mesh->Submeshes[i];
                Handle<Assets::MaterialData> material;
                if (submesh.MaterialIndex < instance.Materials.size()) {
                    material = instance.Materials[submesh.MaterialIndex];
                } else if (submesh.MaterialIndex < mesh->Materials.size()) {
                    material = mesh->Materials[submesh.MaterialIndex];
                }

                AABB worldBounds = TransformBounds(mesh->SubmeshBounds[i], instance.Transform);
                m_SceneBounds.Merge(worldBounds);
                m_DrawItems.push_back({&m_GpuCache->GetMaterial(material), mesh, &submesh, &instance, worldBounds});
            }
        }

        // Grouping by material, then mesh, keeps bind group and buffer changes to a minimum.
        std::sort(m_DrawItems.begin(), m_DrawItems.end(), [](const DrawItem& a, const DrawItem& b) {
            if (a.Material != b.Material) return std::less<const GpuMaterial*>()(a.Material, b.Material);
            return std::less<const GpuMesh*>()(a.Mesh, b.Mesh);
        });
    }

    void ForwardPath::UpdateEnvironment(const RenderScene& scene) {
        const EnvironmentDesc& environment = scene.GetEnvironment();
        const GpuEnvironment& maps = m_GpuCache->GetEnvironmentMaps(environment.Source);
        if (&maps == m_BoundEnvironmentMaps) return;

        // A frame in flight may still be reading the group being replaced.
        m_Device->WaitIdle();

        RHI::BindGroupDesc groupDesc;
        groupDesc.Layout = m_EnvironmentLayout.get();
        groupDesc.DebugName = "EnvironmentGroup";

        auto bind = [&](u32 binding, RHI::RHITexture* texture, RHI::RHISampler* sampler) {
            RHI::BoundResource resource;
            resource.Binding = binding;
            resource.Texture = texture;
            resource.Sampler = sampler;
            groupDesc.Resources.push_back(resource);
        };
        bind(kIrradianceBinding, maps.Irradiance.get(), m_EnvironmentSampler.get());
        bind(kPrefilterBinding, maps.Prefilter.get(), m_EnvironmentSampler.get());
        bind(kBrdfLutBinding, maps.BrdfLut.get(), m_EnvironmentSampler.get());
        bind(kRadianceBinding, maps.Radiance.get(), m_EnvironmentSampler.get());
        bind(kShadowMapBinding, &m_ShadowPass->GetShadowMaps(), &m_ShadowPass->GetSampler());
        bind(kPointShadowMapBinding, &m_ShadowPass->GetPointShadowMaps(), &m_ShadowPass->GetSampler());

        m_EnvironmentGroup = RHI::RHIBindGroup::Create(*m_Device, groupDesc);
        m_BoundEnvironmentMaps = &maps;
    }

    void ForwardPath::WriteFrameUniforms(u32 frameIndex, const RenderScene& scene, const RenderView& view) {
        f32 aspect = static_cast<f32>(m_Swapchain->GetWidth()) / static_cast<f32>(m_Swapchain->GetHeight());

        FrameUniforms frame{};
        frame.View = view.View;
        frame.Projection = glm::perspectiveRH_ZO(view.FieldOfView, aspect, view.NearPlane, view.FarPlane);
        frame.CameraPosition = Vec4(view.Position, 1.0f);
        frame.EnvironmentParameters = Vec4(scene.GetEnvironment().Intensity, 0.0f, 0.0f, 0.0f);

        const std::vector<LightDesc>& lights = scene.GetLights().Values();
        if (lights.size() > kMaxLights && !m_LightLimitWarned) {
            VKE_CORE_WARN("Renderer: scene has {} lights, only the first {} are used", lights.size(), kMaxLights);
            m_LightLimitWarned = true;
        }

        u32 lightCount = static_cast<u32>(std::min<size_t>(lights.size(), kMaxLights));
        frame.LightCount = UVec4(lightCount, 0, 0, 0);
        for (u32 i = 0; i < lightCount; ++i) {
            const LightDesc& light = lights[i];
            GpuLight& gpuLight = frame.Lights[i];
            gpuLight.PositionAndRange = Vec4(light.Position, light.Range);
            gpuLight.DirectionAndType = Vec4(SafeNormalize(light.Direction), static_cast<f32>(light.Type));
            gpuLight.ColorAndIntensity = Vec4(light.Color, light.Intensity);
            gpuLight.SpotAndShadow =
                Vec4(std::cos(light.InnerConeAngle), std::cos(light.OuterConeAngle), -1.0f, 0.0f);
        }

        if (m_ShadowsEnabled && !m_DrawItems.empty()) {
            m_ShadowPass->Prepare(lights, lightCount, m_SceneBounds, frame);
        } else {
            m_ShadowPass->Prepare({}, 0, m_SceneBounds, frame);
        }

        m_FrameUniformBuffers[frameIndex]->Write(&frame, sizeof(frame));
    }

    void ForwardPath::Render(const RenderScene& scene, const RenderView& view) {
        if (m_Window.IsMinimized()) return;

        if (m_Window.ConsumeResizedFlag()) {
            RecreateSwapchain();
        }

        // Uploads any assets seen for the first time before recording starts.
        CollectDrawItems(scene);
        UpdateEnvironment(scene);

        u32 imageIndex;
        if (!m_Swapchain->AcquireNextImage(imageIndex)) {
            RecreateSwapchain();
            return;
        }

        u32 frameIndex = m_Swapchain->GetCurrentFrameIndex();
        WriteFrameUniforms(frameIndex, scene, view);

        RHI::ClearValue sceneClear;
        sceneClear.R = 0.02f;
        sceneClear.G = 0.02f;
        sceneClear.B = 0.05f;

        RHI::RHICommandBuffer& cmd = *m_CommandBuffers[frameIndex];
        cmd.Begin();

        m_ShadowPass->Record(cmd, m_DrawItems);

        cmd.BeginRenderPass(*m_ScenePass, *m_SceneTargets[frameIndex].Framebuffer, sceneClear);
        cmd.BindPipeline(*m_Pipeline);
        cmd.BindBindGroup(kFrameSet, *m_FrameGroups[frameIndex]);
        cmd.BindBindGroup(kEnvironmentSet, *m_EnvironmentGroup);

        const GpuMaterial* boundMaterial = nullptr;
        const GpuMesh* boundMesh = nullptr;
        for (const DrawItem& item : m_DrawItems) {
            if (item.Material != boundMaterial) {
                cmd.BindBindGroup(kMaterialSet, *item.Material->BindGroup);
                boundMaterial = item.Material;
            }
            if (item.Mesh != boundMesh) {
                cmd.BindVertexBuffer(*item.Mesh->VertexBuffer);
                cmd.BindIndexBuffer(*item.Mesh->IndexBuffer, RHI::IndexType::U32);
                boundMesh = item.Mesh;
            }

            ObjectPushConstants object;
            object.Model = item.Instance->Transform;
            Mat3 normalMatrix = glm::transpose(glm::inverse(Mat3(object.Model)));
            for (int column = 0; column < 3; ++column) {
                object.NormalMatrix[column] = Vec4(normalMatrix[column], 0.0f);
            }

            cmd.PushConstants(RHI::ShaderStageFlags::Vertex, 0, sizeof(object), &object);
            cmd.DrawIndexed(item.Submesh->IndexCount, 1, item.Submesh->FirstIndex, 0, 0);
        }

        // Last in the pass, so it only touches pixels the scene left empty.
        m_SkyboxPass->Record(cmd, *m_FrameGroups[frameIndex], *m_EnvironmentGroup);

        cmd.EndRenderPass();

        RHI::ClearValue outputClear;
        cmd.BeginRenderPass(*m_OutputPass, *m_OutputFramebuffers[imageIndex], outputClear);
        m_TonemapPass->Record(cmd, frameIndex, m_Exposure);
        cmd.EndRenderPass();

        cmd.End();

        if (!m_Swapchain->SubmitAndPresent(cmd)) {
            RecreateSwapchain();
        }
    }

} // namespace VKE::Renderer
