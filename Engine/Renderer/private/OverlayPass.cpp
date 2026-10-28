#include "OverlayPass.h"

#include <platform/FileSystem.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace VKE::Renderer {

    namespace {
        // Must match OverlayConstants and the flags in shaders/overlay.frag.
        struct OverlayConstants {
            Vec2 Scale;
            Vec2 Offset;
            u32 Flags;
        };
        constexpr u32 kTextureIsLinear = 1;
        constexpr u32 kOutputEncodes = 2;

        // Formats whose samples come back as linear color rather than the stored values.
        bool SamplesAsLinear(RHI::Format format) {
            switch (format) {
                case RHI::Format::R8G8B8A8_SRGB:
                case RHI::Format::B8G8R8A8_SRGB:
                case RHI::Format::R16G16B16A16_SFLOAT:
                case RHI::Format::R32G32B32A32_SFLOAT: return true;
                default: return false;
            }
        }

        // Room for `needed`, with some to spare so the buffer is not remade every time it grows.
        size_t GrownSize(size_t needed) {
            return std::max<size_t>(needed + needed / 2, 64 * 1024);
        }
    } // namespace

    OverlayPass::OverlayPass(RHI::RHIDevice& device, RHI::RHIRenderPass& outputPass, u32 framesInFlight)
        : m_Device(device), m_FrameBuffers(framesInFlight) {
        RHI::BindGroupLayoutDesc layoutDesc;
        layoutDesc.Slots = {{0, RHI::BindingType::CombinedImageSampler, RHI::ShaderStageFlags::Fragment}};
        layoutDesc.DebugName = "OverlayLayout";
        m_Layout = RHI::RHIBindGroupLayout::Create(m_Device, layoutDesc);

        RHI::SamplerDesc samplerDesc;
        samplerDesc.AddressModeU = RHI::SamplerAddressMode::ClampToEdge;
        samplerDesc.AddressModeV = RHI::SamplerAddressMode::ClampToEdge;
        samplerDesc.MaxAnisotropy = 1.0f;
        samplerDesc.DebugName = "OverlaySampler";
        m_Sampler = RHI::RHISampler::Create(m_Device, samplerDesc);

        const auto shaderDir = Platform::FileSystem::GetExecutableDirectory() / "shaders";

        RHI::PipelineDesc pipelineDesc;
        pipelineDesc.VertexShaderPath = (shaderDir / "overlay.vert.spv").string();
        pipelineDesc.FragmentShaderPath = (shaderDir / "overlay.frag.spv").string();
        pipelineDesc.VertexStride = sizeof(OverlayVertex);
        pipelineDesc.VertexAttributes = {
            {0, RHI::Format::R32G32_SFLOAT, offsetof(OverlayVertex, Position)},
            {1, RHI::Format::R32G32_SFLOAT, offsetof(OverlayVertex, UV)},
            {2, RHI::Format::R8G8B8A8_UNORM, offsetof(OverlayVertex, Color)},
        };
        pipelineDesc.BindGroupLayouts = {m_Layout.get()};
        pipelineDesc.PushConstantRanges = {{RHI::ShaderStageFlags::AllGraphics, 0, sizeof(OverlayConstants)}};
        pipelineDesc.CullMode = RHI::CullMode::None;
        pipelineDesc.Blend = RHI::BlendMode::Alpha;
        m_Pipeline = RHI::RHIPipeline::Create(m_Device, outputPass, pipelineDesc);

        const u32 white = 0xFFFFFFFF;
        RHI::TextureDesc whiteDesc;
        whiteDesc.Format = RHI::Format::R8G8B8A8_UNORM;
        whiteDesc.DebugName = "OverlayWhite";
        m_White = RHI::RHITexture::Create(m_Device, whiteDesc, &white);
    }

    OverlayPass::~OverlayPass() = default;

    RHI::RHIBindGroup& OverlayPass::GetBindGroup(RHI::RHITexture& texture) {
        Iunique<RHI::RHIBindGroup>& group = m_BindGroups[&texture];
        if (!group) {
            RHI::BindGroupDesc groupDesc;
            groupDesc.Layout = m_Layout.get();
            groupDesc.DebugName = "OverlayTexture";
            RHI::BoundResource resource;
            resource.Binding = 0;
            resource.Texture = &texture;
            resource.Sampler = m_Sampler.get();
            groupDesc.Resources = {resource};
            group = RHI::RHIBindGroup::Create(m_Device, groupDesc);
        }
        return *group;
    }

    void OverlayPass::ForgetTexture(RHI::RHITexture& texture) {
        m_BindGroups.erase(&texture);
    }

    void OverlayPass::Upload(FrameBuffers& buffers, const OverlayDrawList* lists, size_t count, size_t vertexCount,
                             size_t indexCount) {
        m_Vertices.clear();
        m_Indices.clear();
        m_Vertices.reserve(vertexCount);
        m_Indices.reserve(indexCount);
        for (size_t i = 0; i < count; ++i) {
            const OverlayDrawList& list = lists[i];
            m_Vertices.insert(m_Vertices.end(), list.Vertices.begin(), list.Vertices.end());
            m_Indices.insert(m_Indices.end(), list.Indices.begin(), list.Indices.end());
        }

        size_t vertexBytes = vertexCount * sizeof(OverlayVertex);
        if (!buffers.Vertices || buffers.Vertices->GetSize() < vertexBytes) {
            RHI::BufferDesc desc;
            desc.Size = GrownSize(vertexBytes);
            desc.Usage = RHI::BufferUsage::Vertex;
            desc.Memory = RHI::BufferMemory::HostVisible;
            desc.DebugName = "OverlayVertices";
            buffers.Vertices = RHI::RHIBuffer::Create(m_Device, desc);
        }
        size_t indexBytes = indexCount * sizeof(u16);
        if (!buffers.Indices || buffers.Indices->GetSize() < indexBytes) {
            RHI::BufferDesc desc;
            desc.Size = GrownSize(indexBytes);
            desc.Usage = RHI::BufferUsage::Index;
            desc.Memory = RHI::BufferMemory::HostVisible;
            desc.DebugName = "OverlayIndices";
            buffers.Indices = RHI::RHIBuffer::Create(m_Device, desc);
        }

        buffers.Vertices->Write(m_Vertices.data(), vertexBytes);
        buffers.Indices->Write(m_Indices.data(), indexBytes);
    }

    void OverlayPass::Record(RHI::RHICommandBuffer& cmd, u32 frameIndex, const OverlayDrawList* lists, size_t count,
                             u32 width, u32 height, bool outputEncodes, const TextureResolver& resolve) {
        size_t vertexCount = 0;
        size_t indexCount = 0;
        for (size_t i = 0; i < count; ++i) {
            vertexCount += lists[i].Vertices.size();
            indexCount += lists[i].Indices.size();
        }
        if (indexCount == 0 || width == 0 || height == 0) return;

        FrameBuffers& buffers = m_FrameBuffers[frameIndex];
        Upload(buffers, lists, count, vertexCount, indexCount);

        cmd.BindPipeline(*m_Pipeline);
        cmd.BindVertexBuffer(*buffers.Vertices);
        cmd.BindIndexBuffer(*buffers.Indices, RHI::IndexType::U16);

        // Clip space is Y-up: the top-left pixel maps to (-1, 1).
        OverlayConstants constants;
        constants.Scale = Vec2(2.0f / static_cast<f32>(width), -2.0f / static_cast<f32>(height));
        constants.Offset = Vec2(-1.0f, 1.0f);

        RHI::RHITexture* boundTexture = nullptr;
        u32 boundFlags = ~0u;
        u32 vertexBase = 0;
        u32 indexBase = 0;
        for (size_t i = 0; i < count; ++i) {
            const OverlayDrawList& list = lists[i];
            for (const OverlayCommand& command : list.Commands) {
                f32 minX = std::max(command.ClipRect.x, 0.0f);
                f32 minY = std::max(command.ClipRect.y, 0.0f);
                f32 maxX = std::min(command.ClipRect.z, static_cast<f32>(width));
                f32 maxY = std::min(command.ClipRect.w, static_cast<f32>(height));
                if (maxX <= minX || maxY <= minY || command.IndexCount == 0) continue;

                RHI::RHITexture* texture = resolve(command.Texture);
                if (!texture) texture = m_White.get();
                u32 flags = (SamplesAsLinear(texture->GetFormat()) ? kTextureIsLinear : 0) |
                            (outputEncodes ? kOutputEncodes : 0);

                if (texture != boundTexture) {
                    cmd.BindBindGroup(0, GetBindGroup(*texture));
                    boundTexture = texture;
                }
                if (flags != boundFlags) {
                    constants.Flags = flags;
                    cmd.PushConstants(RHI::ShaderStageFlags::AllGraphics, 0, sizeof(constants), &constants);
                    boundFlags = flags;
                }

                RHI::ScissorRect scissor;
                scissor.X = static_cast<i32>(minX);
                scissor.Y = static_cast<i32>(minY);
                scissor.Width = static_cast<u32>(std::ceil(maxX)) - static_cast<u32>(minX);
                scissor.Height = static_cast<u32>(std::ceil(maxY)) - static_cast<u32>(minY);
                cmd.SetScissor(scissor);

                cmd.DrawIndexed(command.IndexCount, 1, indexBase + command.FirstIndex,
                                static_cast<i32>(vertexBase) + command.VertexOffset, 0);
            }
            vertexBase += static_cast<u32>(list.Vertices.size());
            indexBase += static_cast<u32>(list.Indices.size());
        }

        RHI::ScissorRect full;
        full.Width = width;
        full.Height = height;
        cmd.SetScissor(full);
    }

} // namespace VKE::Renderer
