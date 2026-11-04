#include "WorldPass.h"

#include "TextureFormats.h"

#include <platform/FileSystem.h>

#include <algorithm>
#include <cstddef>

namespace VKE {

namespace {
// Must match WorldConstants and the flags in shaders/world.vert and world.frag.
struct WorldConstants {
	Mat4 ViewProjection;
	Vec2 ViewportSize;
	Vec2 Fade;
	Vec3 CameraPosition;
	u32 Flags;
};
static_assert(sizeof(WorldConstants) == 96, "WorldConstants must match shaders/world.vert");
constexpr u32 kLines = 1;
constexpr u32 kTextureIsLinear = 2;

// Room for `needed`, with some to spare so blocks are not added every frame.
size_t GrownSize(size_t needed) {
	return std::max<size_t>(needed + needed / 2, 64 * 1024);
}
} // namespace

WorldPass::WorldPass(RHIDevice &device, RHIRenderPass &renderPass, u32 framesInFlight) :
		m_Device(device), m_Blocks(framesInFlight) {
	BindGroupLayoutDesc layoutDesc;
	layoutDesc.Slots = { { 0, BindingType::CombinedImageSampler, ShaderStageFlags::Fragment } };
	layoutDesc.DebugName = "WorldLayout";
	m_Layout = RHIBindGroupLayout::Create(m_Device, layoutDesc);

	SamplerDesc samplerDesc;
	samplerDesc.AddressModeU = SamplerAddressMode::ClampToEdge;
	samplerDesc.AddressModeV = SamplerAddressMode::ClampToEdge;
	samplerDesc.MaxAnisotropy = 1.0f;
	samplerDesc.DebugName = "WorldSampler";
	m_Sampler = RHISampler::Create(m_Device, samplerDesc);

	const auto shaderDir = FileSystem::GetExecutableDirectory() / "shaders";

	PipelineDesc pipelineDesc;
	pipelineDesc.VertexShaderPath = (shaderDir / "world.vert.spv").string();
	pipelineDesc.FragmentShaderPath = (shaderDir / "world.frag.spv").string();
	pipelineDesc.VertexStride = sizeof(WorldVertex);
	pipelineDesc.VertexAttributes = {
		{ 0, Format::R32G32B32_SFLOAT, offsetof(WorldVertex, Position) },
		{ 1, Format::R32G32B32_SFLOAT, offsetof(WorldVertex, Other) },
		{ 2, Format::R32G32_SFLOAT, offsetof(WorldVertex, UV) },
		{ 3, Format::R8G8B8A8_UNORM, offsetof(WorldVertex, Color) },
		{ 4, Format::R32_SFLOAT, offsetof(WorldVertex, Width) },
	};
	pipelineDesc.BindGroupLayouts = { m_Layout.get() };
	pipelineDesc.PushConstantRanges = { { ShaderStageFlags::AllGraphics, 0, sizeof(WorldConstants) } };
	pipelineDesc.CullMode = CullMode::None;
	pipelineDesc.Blend = BlendMode::Alpha;
	m_OnTopPipeline = RHIPipeline::Create(m_Device, renderPass, pipelineDesc);

	pipelineDesc.DepthTestEnable = true;
	pipelineDesc.DepthCompareOp = CompareOp::LessOrEqual;
	m_TestedPipeline = RHIPipeline::Create(m_Device, renderPass, pipelineDesc);

	const u32 white = 0xFFFFFFFF;
	TextureDesc whiteDesc;
	whiteDesc.Format = Format::R8G8B8A8_UNORM;
	whiteDesc.DebugName = "WorldWhite";
	m_White = RHITexture::Create(m_Device, whiteDesc, &white);
}

WorldPass::~WorldPass() = default;

RHIBindGroup &WorldPass::GetBindGroup(RHITexture &texture) {
	Iunique<RHIBindGroup> &group = m_BindGroups[&texture];
	if (!group) {
		BindGroupDesc groupDesc;
		groupDesc.Layout = m_Layout.get();
		groupDesc.DebugName = "WorldTexture";
		BoundResource resource;
		resource.Binding = 0;
		resource.Texture = &texture;
		resource.Sampler = m_Sampler.get();
		groupDesc.Resources = { resource };
		group = RHIBindGroup::Create(m_Device, groupDesc);
	}
	return *group;
}

void WorldPass::ForgetTexture(RHITexture &texture) {
	m_BindGroups.erase(&texture);
}

WorldPass::Block WorldPass::MakeBlock(size_t vertexBytes, size_t indexBytes) {
	Block block;
	BufferDesc vertexDesc;
	vertexDesc.Size = GrownSize(vertexBytes);
	vertexDesc.Usage = BufferUsage::Vertex;
	vertexDesc.Memory = BufferMemory::HostVisible;
	vertexDesc.DebugName = "WorldVertices";
	block.Vertices = RHIBuffer::Create(m_Device, vertexDesc);

	BufferDesc indexDesc;
	indexDesc.Size = GrownSize(indexBytes);
	indexDesc.Usage = BufferUsage::Index;
	indexDesc.Memory = BufferMemory::HostVisible;
	indexDesc.DebugName = "WorldIndices";
	block.Indices = RHIBuffer::Create(m_Device, indexDesc);
	return block;
}

void WorldPass::BeginFrame(u32 frameIndex) {
	std::vector<Block> &blocks = m_Blocks[frameIndex];
	if (blocks.size() > 1) {
		size_t vertexBytes = 0;
		size_t indexBytes = 0;
		for (const Block &block : blocks) {
			vertexBytes += block.Vertices->GetSize();
			indexBytes += block.Indices->GetSize();
		}
		blocks.clear();
		blocks.push_back(MakeBlock(vertexBytes, indexBytes));
	}
	for (Block &block : blocks) {
		block.VerticesUsed = 0;
		block.IndicesUsed = 0;
	}
}

void WorldPass::Record(RHICommandBuffer &cmd, u32 frameIndex, const WorldDrawList &list, const Mat4 &viewProjection,
		const Vec3 &cameraPosition, u32 width, u32 height, const TextureResolver &resolve) {
	if (list.Indices.empty() || width == 0 || height == 0) {
		return;
	}

	size_t vertexBytes = list.Vertices.size() * sizeof(WorldVertex);
	size_t indexBytes = list.Indices.size() * sizeof(u32);
	std::vector<Block> &blocks = m_Blocks[frameIndex];
	if (blocks.empty() || blocks.back().VerticesUsed + vertexBytes > blocks.back().Vertices->GetSize() ||
			blocks.back().IndicesUsed + indexBytes > blocks.back().Indices->GetSize()) {
		blocks.push_back(MakeBlock(vertexBytes, indexBytes));
	}
	Block &block = blocks.back();
	size_t vertexOffset = block.VerticesUsed;
	size_t indexOffset = block.IndicesUsed;
	block.Vertices->Write(list.Vertices.data(), vertexBytes, vertexOffset);
	block.Indices->Write(list.Indices.data(), indexBytes, indexOffset);
	block.VerticesUsed += vertexBytes;
	block.IndicesUsed += indexBytes;

	WorldConstants constants;
	constants.ViewProjection = viewProjection;
	constants.ViewportSize = Vec2(static_cast<f32>(width), static_cast<f32>(height));
	constants.CameraPosition = cameraPosition;

	RHIPipeline *boundPipeline = nullptr;
	RHITexture *boundTexture = nullptr;
	for (const WorldCommand &command : list.Commands) {
		if (command.IndexCount == 0) {
			continue;
		}

		RHIPipeline *pipeline =
				command.Style.Depth == WorldDepth::Tested ? m_TestedPipeline.get() : m_OnTopPipeline.get();
		if (pipeline != boundPipeline) {
			cmd.BindPipeline(*pipeline);
			cmd.BindVertexBuffer(*block.Vertices, vertexOffset);
			cmd.BindIndexBuffer(*block.Indices, IndexType::U32, indexOffset);
			boundPipeline = pipeline;
			boundTexture = nullptr;
		}

		RHITexture *texture = nullptr;
		if (command.Primitive == WorldPrimitive::Triangles) {
			texture = resolve(command.Style.Texture);
		}
		if (!texture) {
			texture = m_White.get();
		}
		if (texture != boundTexture) {
			cmd.BindBindGroup(0, GetBindGroup(*texture));
			boundTexture = texture;
		}

		constants.Fade = Vec2(command.Style.FadeStart, command.Style.FadeEnd);
		constants.Flags = (command.Primitive == WorldPrimitive::Lines ? kLines : 0) |
				(SamplesAsLinear(texture->GetFormat()) ? kTextureIsLinear : 0);
		cmd.PushConstants(ShaderStageFlags::AllGraphics, 0, sizeof(constants), &constants);
		cmd.DrawIndexed(command.IndexCount, 1, command.FirstIndex, 0, 0);
	}
}

} // namespace VKE
