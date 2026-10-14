#pragma once

#include <core/Types.h>
#include <string>
#include <vector>

// Shared types used across the RHI interfaces.

namespace VKE::RHI {

    class RHIBindGroupLayout;

    enum class Format {
        Undefined,
        R32_SFLOAT,
        R32G32_SFLOAT,
        R32G32B32_SFLOAT,
        R32G32B32A32_SFLOAT,
        R16G16_SFLOAT,
        R16G16B16A16_SFLOAT,
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

    enum class ShaderStageFlags : u32 {
        None = 0,
        Vertex = 1 << 0,
        Fragment = 1 << 1,
        AllGraphics = Vertex | Fragment,
    };
    VKE_ENUM_FLAGS(ShaderStageFlags)

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

    // -------------------------------------------------------------------------
    // Buffers
    // -------------------------------------------------------------------------

    enum class BufferUsage : u32 {
        None = 0,
        Vertex = 1 << 0,
        Index = 1 << 1,
        Uniform = 1 << 2,
    };
    VKE_ENUM_FLAGS(BufferUsage)

    enum class BufferMemory {
        // Only the GPU reads it. Contents are provided at creation.
        GpuOnly,
        // The CPU can update it with RHIBuffer::Write.
        HostVisible,
    };

    struct BufferDesc {
        size_t Size = 0;
        BufferUsage Usage = BufferUsage::None;
        BufferMemory Memory = BufferMemory::GpuOnly;
        const char* DebugName = nullptr;
    };

    enum class IndexType {
        U16,
        U32,
    };

    // -------------------------------------------------------------------------
    // Textures and samplers
    // -------------------------------------------------------------------------

    enum class TextureUsage : u32 {
        None = 0,
        Sampled = 1 << 0,
        ColorAttachment = 1 << 1,
        DepthAttachment = 1 << 2,
    };
    VKE_ENUM_FLAGS(TextureUsage)

    enum class TextureType {
        Texture2D,
        // Six layers per cube, ordered +X, -X, +Y, -Y, +Z, -Z.
        TextureCube,
    };

    struct TextureDesc {
        u32 Width = 1;
        u32 Height = 1;
        u32 ArrayLayers = 1;
        // 0 allocates the full mip chain.
        u32 MipLevels = 1;
        TextureType Type = TextureType::Texture2D;
        Format Format = Format::R8G8B8A8_UNORM;
        TextureUsage Usage = TextureUsage::Sampled;
        // Fills the mip chain from the initial data.
        bool GenerateMips = false;
        const char* DebugName = nullptr;
    };

    enum class Filter { Nearest, Linear };
    enum class SamplerAddressMode { Repeat, MirroredRepeat, ClampToEdge, ClampToBorder };

    struct SamplerDesc {
        Filter MinFilter = Filter::Linear;
        Filter MagFilter = Filter::Linear;
        Filter MipFilter = Filter::Linear;
        SamplerAddressMode AddressModeU = SamplerAddressMode::Repeat;
        SamplerAddressMode AddressModeV = SamplerAddressMode::Repeat;
        SamplerAddressMode AddressModeW = SamplerAddressMode::Repeat;
        // Clamped to the device limit. 1 disables anisotropic filtering.
        f32 MaxAnisotropy = 16.0f;
        // Sampling returns how much of the filtered area passes the comparison
        // against a reference depth, instead of the stored value.
        bool CompareEnable = false;
        CompareOp CompareOp = CompareOp::LessOrEqual;
        const char* DebugName = nullptr;
    };

    // -------------------------------------------------------------------------
    // Bind groups
    // -------------------------------------------------------------------------

    enum class BindingType {
        UniformBuffer,
        // Offset is chosen when the bind group is bound.
        UniformBufferDynamic,
        CombinedImageSampler,
    };

    // -------------------------------------------------------------------------
    // Render pass / framebuffer
    // -------------------------------------------------------------------------

    struct AttachmentDesc {
        Format Format = Format::Undefined;
        bool IsDepth = false;
        // If false, the previous contents are kept instead of cleared.
        bool Clear = true;
        // Set on color attachments that are presented to the screen.
        bool PresentAfter = false;
        // Leaves the attachment readable by shaders once the pass ends.
        bool SampledAfter = false;
    };

    struct RenderPassDesc {
        std::vector<AttachmentDesc> ColorAttachments;
        bool HasDepthAttachment = false;
        AttachmentDesc DepthAttachment;
    };

    struct FramebufferDesc {
        // One view per render pass attachment, colors first and depth last.
        std::vector<void*> AttachmentViews;
        u32 Width = 0;
        u32 Height = 0;
    };

    struct ClearValue {
        f32 R = 0.0f, G = 0.0f, B = 0.0f, A = 1.0f;
        f32 Depth = 1.0f;
    };

    // Clip space is Y-up, with depth in [0, 1].
    struct Viewport {
        f32 X = 0.0f, Y = 0.0f;
        f32 Width = 0.0f, Height = 0.0f;
        f32 MinDepth = 0.0f, MaxDepth = 1.0f;
    };

    struct ScissorRect {
        i32 X = 0, Y = 0;
        u32 Width = 0, Height = 0;
    };

    // -------------------------------------------------------------------------
    // Pipeline
    // -------------------------------------------------------------------------

    struct VertexAttribute {
        u32 Location = 0;
        Format Format = Format::R32G32B32_SFLOAT;
        u32 Offset = 0;
    };

    struct PushConstantRange {
        ShaderStageFlags Stages = ShaderStageFlags::None;
        u32 Offset = 0;
        u32 Size = 0;
    };

    struct PipelineDesc {
        std::string VertexShaderPath;
        // Empty for passes that only write depth.
        std::string FragmentShaderPath;

        // Single interleaved vertex binding. A stride of 0 declares no vertex
        // input at all, for passes whose shaders build their own vertices.
        std::vector<VertexAttribute> VertexAttributes;
        u32 VertexStride = 0;

        // Layout for each bind group index. nullptr leaves that index unused.
        std::vector<RHIBindGroupLayout*> BindGroupLayouts;
        std::vector<PushConstantRange> PushConstantRanges;

        PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
        PolygonMode PolygonMode = PolygonMode::Fill;
        CullMode CullMode = CullMode::Back;
        FrontFace FrontFace = FrontFace::CounterClockwise;

        bool DepthTestEnable = false;
        bool DepthWriteEnable = false;
        CompareOp DepthCompareOp = CompareOp::Less;

        // Pushes depth away from the light when rendering shadow maps.
        bool DepthBiasEnable = false;
        f32 DepthBiasConstant = 0.0f;
        f32 DepthBiasSlope = 0.0f;
    };

    // -------------------------------------------------------------------------
    // Device
    // -------------------------------------------------------------------------

    struct DeviceLimits {
        u32 MinUniformBufferOffsetAlignment = 1;
        u32 MaxPushConstantSize = 0;
        f32 MaxSamplerAnisotropy = 1.0f;
    };

} // namespace VKE::RHI
