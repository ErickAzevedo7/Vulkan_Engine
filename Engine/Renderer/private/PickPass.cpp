#include "PickPass.h"

#include <platform/FileSystem.h>
#include <renderer/RenderScene.h>

#include <cstddef>

namespace VKE {

namespace {
constexpr Format kIDFormat = Format::R32G32_UINT;

// Must match PickConstants in shaders/pick.vert and pick.frag.
struct PickConstants {
	Mat4 ModelViewProjection;
	// The instance's index + 1, so 0 means nothing, and its generation.
	u32 ID[2];
};
static_assert(sizeof(PickConstants) == 72, "PickConstants must match shaders/pick.vert");
} // namespace

PickPass::PickPass(RHIDevice &device, GpuCache &gpuCache) : m_Device(device), m_GpuCache(gpuCache) {
	RenderPassDesc passDesc;
	AttachmentDesc idAttachment;
	idAttachment.Format = kIDFormat;
	idAttachment.CopiedAfter = true;
	passDesc.ColorAttachments = { idAttachment };
	passDesc.HasDepthAttachment = true;
	passDesc.DepthAttachment.Format = m_Device.GetDepthFormat();
	passDesc.DepthAttachment.IsDepth = true;
	m_RenderPass = RHIRenderPass::Create(m_Device, passDesc);

	const auto shaderDir = FileSystem::GetExecutableDirectory() / "shaders";

	PipelineDesc pipelineDesc;
	pipelineDesc.VertexShaderPath = (shaderDir / "pick.vert.spv").string();
	pipelineDesc.FragmentShaderPath = (shaderDir / "pick.frag.spv").string();
	pipelineDesc.VertexStride = sizeof(Vertex);
	pipelineDesc.VertexAttributes = { { 0, Format::R32G32B32_SFLOAT, offsetof(Vertex, Position) } };
	pipelineDesc.PushConstantRanges = { { ShaderStageFlags::AllGraphics, 0, sizeof(PickConstants) } };
	pipelineDesc.CullMode = CullMode::Back;
	pipelineDesc.FrontFace = FrontFace::CounterClockwise;
	pipelineDesc.DepthTestEnable = true;
	pipelineDesc.DepthWriteEnable = true;
	m_Pipeline = RHIPipeline::Create(m_Device, *m_RenderPass, pipelineDesc);
}

PickPass::~PickPass() = default;

Iunique<PickTarget> PickPass::CreateTarget(u32 width, u32 height) {
	auto target = MakeIunique<PickTarget>();
	target->Width = width;
	target->Height = height;

	TextureDesc idDesc;
	idDesc.Width = width;
	idDesc.Height = height;
	idDesc.Format = kIDFormat;
	idDesc.Usage = TextureUsage::ColorAttachment | TextureUsage::CopySource;
	idDesc.DebugName = "PickIDs";
	target->IDs = RHITexture::Create(m_Device, idDesc);

	TextureDesc depthDesc;
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.Format = m_Device.GetDepthFormat();
	depthDesc.Usage = TextureUsage::DepthAttachment;
	depthDesc.DebugName = "PickDepth";
	target->Depth = RHITexture::Create(m_Device, depthDesc);

	FramebufferDesc framebufferDesc;
	framebufferDesc.AttachmentViews = { target->IDs->GetAttachmentView(0, 0), target->Depth->GetAttachmentView(0, 0) };
	framebufferDesc.Width = width;
	framebufferDesc.Height = height;
	target->Framebuffer = RHIFramebuffer::Create(m_Device, *m_RenderPass, framebufferDesc);

	BufferDesc readbackDesc;
	readbackDesc.Size = 2 * sizeof(u32);
	readbackDesc.Memory = BufferMemory::Readback;
	readbackDesc.DebugName = "PickReadback";
	target->Readback = RHIBuffer::Create(m_Device, readbackDesc);
	return target;
}

void PickPass::Record(
		RHICommandBuffer &cmd, const RenderScene &scene, const Mat4 &viewProjection, PickTarget &target, u32 x, u32 y) {
	if (x >= target.Width || y >= target.Height) {
		return;
	}

	// Cleared to 0, which is no instance.
	ClearValue clear;
	clear.A = 0.0f;
	cmd.BeginRenderPass(*m_RenderPass, *target.Framebuffer, clear);

	ScissorRect pixel;
	pixel.X = static_cast<i32>(x);
	pixel.Y = static_cast<i32>(y);
	pixel.Width = 1;
	pixel.Height = 1;
	cmd.SetScissor(pixel);
	cmd.BindPipeline(*m_Pipeline);

	const SlotMap<MeshInstanceDesc> &meshes = scene.GetMeshes();
	for (size_t i = 0; i < meshes.Size(); ++i) {
		const MeshInstanceDesc &instance = meshes.Values()[i];
		if (!instance.Visible) {
			continue;
		}
		const GpuMesh *mesh = m_GpuCache.GetMesh(instance.Mesh);
		if (!mesh) {
			continue;
		}

		MeshInstanceID id = meshes.HandleAt(i);
		PickConstants constants;
		constants.ModelViewProjection = viewProjection * instance.Transform;
		constants.ID[0] = id.Index + 1;
		constants.ID[1] = id.Generation;
		cmd.PushConstants(ShaderStageFlags::AllGraphics, 0, sizeof(constants), &constants);

		cmd.BindVertexBuffer(*mesh->VertexBuffer);
		cmd.BindIndexBuffer(*mesh->IndexBuffer, IndexType::U32);
		for (const Submesh &submesh : mesh->Submeshes) {
			cmd.DrawIndexed(submesh.IndexCount, 1, submesh.FirstIndex, 0, 0);
		}
	}
	cmd.EndRenderPass();

	cmd.CopyTextureToBuffer(*target.IDs, pixel, *target.Readback);
}

MeshInstanceID PickPass::ReadResult(PickTarget &target) {
	u32 id[2] = { 0, 0 };
	target.Readback->Read(id, sizeof(id));
	if (id[0] == 0) {
		return {};
	}
	return { id[0] - 1, id[1] };
}

} // namespace VKE
