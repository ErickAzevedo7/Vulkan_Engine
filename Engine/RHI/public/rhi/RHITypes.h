#pragma once

#include <core/Types.h>
#include <string>
#include <vector>

// Shared types used across the RHI interfaces.

namespace VKE::RHI {

    // Only the subset of formats actually in use (vertex attributes,
    // swapchain color, depth). Extend as real usage demands it.
    enum class Format {
        Undefined,
        R32_SFLOAT,
        R32G32_SFLOAT,
        R32G32B32_SFLOAT,
        R32G32B32A32_SFLOAT,
        R8G8B8A8_UNORM,
        R8G8B8A8_SRGB,
        B8G8R8A8_UNORM,
        B8G8R8A8_SRGB,
        D32_SFLOAT,
        D24_UNORM_S8_UINT,
        D32_SFLOAT_S8_UINT,
    };

    enum class ShaderStageKind {
        Vertex,
        Fragment,
    };

    enum class PrimitiveTopology {
        TriangleList,
        LineList,
        PointList,
    };

    enum class PolygonMode {
        Fill,
        Line,
        Point,
    };

    enum class CullMode {
        None,
        Front,
        Back,
    };

    enum class FrontFace {
        CounterClockwise,
        Clockwise,
    };

    enum class CompareOp {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always,
    };

    enum class BufferUsage {
        Vertex,
        Index,
        Uniform,
    };

    // -------------------------------------------------------------------------
    // Render pass / framebuffer description
    // -------------------------------------------------------------------------

    struct AttachmentDesc {
        Format Format = Format::Undefined;
        bool IsDepth = false;
        // If false, the attachment's previous contents are preserved
        // instead of cleared at pass start.
        bool Clear = true;
        // Color attachments meant for presentation must leave the render
        // pass in a presentable layout; irrelevant for depth attachments.
        bool PresentAfter = false;
    };

    struct RenderPassDesc {
        std::vector<AttachmentDesc> ColorAttachments;
        bool HasDepthAttachment = false;
        AttachmentDesc DepthAttachment;
    };

    struct FramebufferDesc {
        // Opaque attachment views, one per RenderPassDesc attachment, in the
        // same order (color attachments first, depth last if present).
        std::vector<void*> AttachmentViews;
        u32 Width = 0;
        u32 Height = 0;
    };

    struct ClearValue {
        float R = 0.0f, G = 0.0f, B = 0.0f, A = 1.0f;
        float Depth = 1.0f;
    };

    // -------------------------------------------------------------------------
    // Pipeline description
    // -------------------------------------------------------------------------

    struct VertexAttribute {
        u32 Location = 0;
        Format Format = Format::R32G32B32_SFLOAT;
        u32 Offset = 0;
    };

    struct PipelineDesc {
        std::string VertexShaderPath;
        std::string FragmentShaderPath;

        // Single vertex binding, describing one interleaved vertex struct.
        // Multiple bindings aren't supported yet.
        std::vector<VertexAttribute> VertexAttributes;
        u32 VertexStride = 0;

        PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
        PolygonMode PolygonMode = PolygonMode::Fill;
        CullMode CullMode = CullMode::Back;
        FrontFace FrontFace = FrontFace::CounterClockwise;

        bool DepthTestEnable = false;
        bool DepthWriteEnable = false;
        CompareOp DepthCompareOp = CompareOp::Less;
    };

    // -------------------------------------------------------------------------
    // Texture / sampler description
    // -------------------------------------------------------------------------

    struct TextureDesc {
        u32 Width = 1;
        u32 Height = 1;
        Format Format = Format::R8G8B8A8_UNORM;
        const char* DebugName = nullptr;
    };

    enum class Filter { Nearest, Linear };
    enum class SamplerAddressMode { Repeat, MirroredRepeat, ClampToEdge, ClampToBorder };

    struct SamplerDesc {
        Filter MinFilter = Filter::Linear;
        Filter MagFilter = Filter::Linear;
        SamplerAddressMode AddressModeU = SamplerAddressMode::Repeat;
        SamplerAddressMode AddressModeV = SamplerAddressMode::Repeat;
    };

    // Placeholder — real binding-layout design is unwritten.
    struct DescriptorSetLayoutDesc {
        const char* DebugName = nullptr;
    };

} // namespace VKE::RHI
