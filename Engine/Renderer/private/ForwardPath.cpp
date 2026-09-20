#include "ForwardPath.h"
#include "ShaderLayouts.h"

#include <assets/AssetManager.h>
#include <core/Log.h>
#include <platform/FileSystem.h>
#include <platform/Window.h>
#include <renderer/RenderScene.h>

#include <algorithm>
#include <cstddef>
#include <functional>

namespace VKE::Renderer {

    namespace {
        static_assert(sizeof(Assets::Vertex) == 48, "Vertex layout changed, update the pipeline attributes");

        Vec3 SafeNormalize(const Vec3& v) {
            f32 length = glm::length(v);
            return length > 0.0f ? v / length : Vec3(0.0f);
        }
    } // namespace

    ForwardPath::ForwardPath(Platform::IWindow& window, Assets::AssetManager& assets) : m_Window(window) {
        m_Device = RHI::RHIDevice::Create(m_Window);
        m_Swapchain = RHI::RHISwapchain::Create(*m_Device, m_Window);

        RHI::RenderPassDesc renderPassDesc;
        RHI::AttachmentDesc colorAttachment;
        colorAttachment.Format = m_Swapchain->GetColorFormat();
        colorAttachment.PresentAfter = true;
        renderPassDesc.ColorAttachments = {colorAttachment};
        renderPassDesc.HasDepthAttachment = true;
        renderPassDesc.DepthAttachment.Format = m_Device->GetDepthFormat();
        renderPassDesc.DepthAttachment.IsDepth = true;
        m_RenderPass = RHI::RHIRenderPass::Create(*m_Device, renderPassDesc);

        CreateSwapchainTargets();

        RHI::BindGroupLayoutDesc frameLayoutDesc;
        frameLayoutDesc.Slots = {{0, RHI::BindingType::UniformBuffer, RHI::ShaderStageFlags::AllGraphics}};
        frameLayoutDesc.DebugName = "FrameLayout";
        m_FrameLayout = RHI::RHIBindGroupLayout::Create(*m_Device, frameLayoutDesc);

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
        desc.BindGroupLayouts = {m_FrameLayout.get(), nullptr, m_MaterialLayout.get()};
        desc.PushConstantRanges = {{RHI::ShaderStageFlags::Vertex, 0, sizeof(ObjectPushConstants)}};
        desc.CullMode = RHI::CullMode::Back;
        desc.FrontFace = RHI::FrontFace::CounterClockwise;
        desc.DepthTestEnable = true;
        desc.DepthWriteEnable = true;
        m_Pipeline = RHI::RHIPipeline::Create(*m_Device, *m_RenderPass, desc);
    }

    void ForwardPath::CreateSwapchainTargets() {
        m_Framebuffers.clear();
        m_DepthTextures.clear();

        u32 width = m_Swapchain->GetWidth();
        u32 height = m_Swapchain->GetHeight();

        // One depth texture per swapchain image, so frames in flight never share one.
        for (u32 i = 0; i < m_Swapchain->GetImageCount(); ++i) {
            RHI::TextureDesc depthDesc;
            depthDesc.Width = width;
            depthDesc.Height = height;
            depthDesc.Format = m_Device->GetDepthFormat();
            depthDesc.Usage = RHI::TextureUsage::DepthAttachment;
            depthDesc.DebugName = "SwapchainDepth";
            m_DepthTextures.push_back(RHI::RHITexture::Create(*m_Device, depthDesc));

            RHI::FramebufferDesc framebufferDesc;
            framebufferDesc.AttachmentViews = {m_Swapchain->GetImageView(i), m_DepthTextures.back()->GetView()};
            framebufferDesc.Width = width;
            framebufferDesc.Height = height;
            m_Framebuffers.push_back(RHI::RHIFramebuffer::Create(*m_Device, *m_RenderPass, framebufferDesc));
        }
    }

    void ForwardPath::RecreateSwapchain() {
        // Framebuffers reference the swapchain views, so release them before recreating.
        m_Device->WaitIdle();
        m_Framebuffers.clear();
        m_DepthTextures.clear();
        m_Swapchain->Recreate();
        CreateSwapchainTargets();
    }

    void ForwardPath::CollectDrawItems(const RenderScene& scene) {
        m_DrawItems.clear();

        for (const MeshInstanceDesc& instance : scene.GetMeshes().Values()) {
            if (!instance.Visible) continue;

            const GpuMesh* mesh = m_GpuCache->GetMesh(instance.Mesh);
            if (!mesh) continue;

            for (const Assets::Submesh& submesh : mesh->Submeshes) {
                Handle<Assets::MaterialData> material;
                if (submesh.MaterialIndex < instance.Materials.size()) {
                    material = instance.Materials[submesh.MaterialIndex];
                } else if (submesh.MaterialIndex < mesh->Materials.size()) {
                    material = mesh->Materials[submesh.MaterialIndex];
                }
                m_DrawItems.push_back({&m_GpuCache->GetMaterial(material), mesh, &submesh, &instance});
            }
        }

        // Grouping by material, then mesh, keeps bind group and buffer changes to a minimum.
        std::sort(m_DrawItems.begin(), m_DrawItems.end(), [](const DrawItem& a, const DrawItem& b) {
            if (a.Material != b.Material) return std::less<const GpuMaterial*>()(a.Material, b.Material);
            return std::less<const GpuMesh*>()(a.Mesh, b.Mesh);
        });
    }

    void ForwardPath::WriteFrameUniforms(u32 frameIndex, const RenderScene& scene, const RenderView& view) {
        f32 aspect = static_cast<f32>(m_Swapchain->GetWidth()) / static_cast<f32>(m_Swapchain->GetHeight());

        FrameUniforms frame{};
        frame.View = view.View;
        frame.Projection = glm::perspectiveRH_ZO(view.FieldOfView, aspect, view.NearPlane, view.FarPlane);
        frame.CameraPosition = Vec4(view.Position, 1.0f);

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

        u32 imageIndex;
        if (!m_Swapchain->AcquireNextImage(imageIndex)) {
            RecreateSwapchain();
            return;
        }

        u32 frameIndex = m_Swapchain->GetCurrentFrameIndex();
        WriteFrameUniforms(frameIndex, scene, view);

        RHI::ClearValue clear;
        clear.R = 0.02f;
        clear.G = 0.02f;
        clear.B = 0.05f;

        RHI::RHICommandBuffer& cmd = *m_CommandBuffers[frameIndex];
        cmd.Begin();
        cmd.BeginRenderPass(*m_RenderPass, *m_Framebuffers[imageIndex], clear);
        cmd.BindPipeline(*m_Pipeline);
        cmd.BindBindGroup(kFrameSet, *m_FrameGroups[frameIndex]);

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

        cmd.EndRenderPass();
        cmd.End();

        if (!m_Swapchain->SubmitAndPresent(cmd)) {
            RecreateSwapchain();
        }
    }

} // namespace VKE::Renderer
