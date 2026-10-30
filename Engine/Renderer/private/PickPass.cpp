#include "PickPass.h"

#include <platform/FileSystem.h>
#include <renderer/RenderScene.h>

#include <cstddef>

namespace VKE::Renderer {

    namespace {
        constexpr RHI::Format kIDFormat = RHI::Format::R32G32_UINT;

        // Must match PickConstants in shaders/pick.vert and pick.frag.
        struct PickConstants {
            Mat4 ModelViewProjection;
            // The instance's index + 1, so 0 means nothing, and its generation.
            u32 ID[2];
        };
        static_assert(sizeof(PickConstants) == 72, "PickConstants must match shaders/pick.vert");
    } // namespace

    PickPass::PickPass(RHI::RHIDevice& device, GpuCache& gpuCache) : m_Device(device), m_GpuCache(gpuCache) {
        RHI::RenderPassDesc passDesc;
        RHI::AttachmentDesc idAttachment;
        idAttachment.Format = kIDFormat;
        idAttachment.CopiedAfter = true;
        passDesc.ColorAttachments = {idAttachment};
        passDesc.HasDepthAttachment = true;
        passDesc.DepthAttachment.Format = m_Device.GetDepthFormat();
        passDesc.DepthAttachment.IsDepth = true;
        m_RenderPass = RHI::RHIRenderPass::Create(m_Device, passDesc);

        const auto shaderDir = Platform::FileSystem::GetExecutableDirectory() / "shaders";

        RHI::PipelineDesc pipelineDesc;
        pipelineDesc.VertexShaderPath = (shaderDir / "pick.vert.spv").string();
        pipelineDesc.FragmentShaderPath = (shaderDir / "pick.frag.spv").string();
        pipelineDesc.VertexStride = sizeof(Assets::Vertex);
        pipelineDesc.VertexAttributes = {{0, RHI::Format::R32G32B32_SFLOAT, offsetof(Assets::Vertex, Position)}};
        pipelineDesc.PushConstantRanges = {{RHI::ShaderStageFlags::AllGraphics, 0, sizeof(PickConstants)}};
        pipelineDesc.CullMode = RHI::CullMode::Back;
        pipelineDesc.FrontFace = RHI::FrontFace::CounterClockwise;
        pipelineDesc.DepthTestEnable = true;
        pipelineDesc.DepthWriteEnable = true;
        m_Pipeline = RHI::RHIPipeline::Create(m_Device, *m_RenderPass, pipelineDesc);
    }

    PickPass::~PickPass() = default;

    Iunique<PickTarget> PickPass::CreateTarget(u32 width, u32 height) {
        auto target = MakeIunique<PickTarget>();
        target->Width = width;
        target->Height = height;

        RHI::TextureDesc idDesc;
        idDesc.Width = width;
        idDesc.Height = height;
        idDesc.Format = kIDFormat;
        idDesc.Usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::CopySource;
        idDesc.DebugName = "PickIDs";
        target->IDs = RHI::RHITexture::Create(m_Device, idDesc);

        RHI::TextureDesc depthDesc;
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.Format = m_Device.GetDepthFormat();
        depthDesc.Usage = RHI::TextureUsage::DepthAttachment;
        depthDesc.DebugName = "PickDepth";
        target->Depth = RHI::RHITexture::Create(m_Device, depthDesc);

        RHI::FramebufferDesc framebufferDesc;
        framebufferDesc.AttachmentViews = {target->IDs->GetAttachmentView(0, 0),
                                           target->Depth->GetAttachmentView(0, 0)};
        framebufferDesc.Width = width;
        framebufferDesc.Height = height;
        target->Framebuffer = RHI::RHIFramebuffer::Create(m_Device, *m_RenderPass, framebufferDesc);

        RHI::BufferDesc readbackDesc;
        readbackDesc.Size = 2 * sizeof(u32);
        readbackDesc.Memory = RHI::BufferMemory::Readback;
        readbackDesc.DebugName = "PickReadback";
        target->Readback = RHI::RHIBuffer::Create(m_Device, readbackDesc);
        return target;
    }

    void PickPass::Record(RHI::RHICommandBuffer& cmd, const RenderScene& scene, const Mat4& viewProjection,
                          PickTarget& target, u32 x, u32 y) {
        if (x >= target.Width || y >= target.Height) return;

        // Cleared to 0, which is no instance.
        RHI::ClearValue clear;
        clear.A = 0.0f;
        cmd.BeginRenderPass(*m_RenderPass, *target.Framebuffer, clear);

        RHI::ScissorRect pixel;
        pixel.X = static_cast<i32>(x);
        pixel.Y = static_cast<i32>(y);
        pixel.Width = 1;
        pixel.Height = 1;
        cmd.SetScissor(pixel);
        cmd.BindPipeline(*m_Pipeline);

        const SlotMap<MeshInstanceDesc>& meshes = scene.GetMeshes();
        for (size_t i = 0; i < meshes.Size(); ++i) {
            const MeshInstanceDesc& instance = meshes.Values()[i];
            if (!instance.Visible) continue;
            const GpuMesh* mesh = m_GpuCache.GetMesh(instance.Mesh);
            if (!mesh) continue;

            MeshInstanceID id = meshes.HandleAt(i);
            PickConstants constants;
            constants.ModelViewProjection = viewProjection * instance.Transform;
            constants.ID[0] = id.Index + 1;
            constants.ID[1] = id.Generation;
            cmd.PushConstants(RHI::ShaderStageFlags::AllGraphics, 0, sizeof(constants), &constants);

            cmd.BindVertexBuffer(*mesh->VertexBuffer);
            cmd.BindIndexBuffer(*mesh->IndexBuffer, RHI::IndexType::U32);
            for (const Assets::Submesh& submesh : mesh->Submeshes) {
                cmd.DrawIndexed(submesh.IndexCount, 1, submesh.FirstIndex, 0, 0);
            }
        }
        cmd.EndRenderPass();

        cmd.CopyTextureToBuffer(*target.IDs, pixel, *target.Readback);
    }

    MeshInstanceID PickPass::ReadResult(PickTarget& target) {
        u32 id[2] = {0, 0};
        target.Readback->Read(id, sizeof(id));
        if (id[0] == 0) return {};
        return {id[0] - 1, id[1]};
    }

} // namespace VKE::Renderer
