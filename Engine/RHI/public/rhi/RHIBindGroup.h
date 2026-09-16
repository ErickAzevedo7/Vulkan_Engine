#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>
#include <cstddef>
#include <vector>

namespace VKE::RHI {

    class RHIDevice;
    class RHIBuffer;
    class RHITexture;
    class RHISampler;

    struct BindGroupLayoutEntry {
        u32 Binding = 0;
        BindingType Type = BindingType::UniformBuffer;
        ShaderStageFlags Stages = ShaderStageFlags::AllGraphics;
    };

    struct BindGroupLayoutDesc {
        std::vector<BindGroupLayoutEntry> Entries;
        const char* DebugName = nullptr;
    };

    // Describes which resources a bind group holds and which shader stages see them.
    class RHIBindGroupLayout {
    public:
        virtual ~RHIBindGroupLayout() = default;

        static Iunique<RHIBindGroupLayout> Create(RHIDevice& device, const BindGroupLayoutDesc& desc);
    };

    struct BindGroupEntry {
        u32 Binding = 0;

        // Uniform buffers. Size 0 binds from Offset to the end of the buffer.
        RHIBuffer* Buffer = nullptr;
        size_t Offset = 0;
        size_t Size = 0;

        // Combined image samplers.
        RHITexture* Texture = nullptr;
        RHISampler* Sampler = nullptr;
    };

    struct BindGroupDesc {
        RHIBindGroupLayout* Layout = nullptr;
        // One entry per layout binding.
        std::vector<BindGroupEntry> Entries;
        const char* DebugName = nullptr;
    };

    // Resources bound together to a pipeline. Immutable: create a new one to change its contents.
    class RHIBindGroup {
    public:
        virtual ~RHIBindGroup() = default;

        static Iunique<RHIBindGroup> Create(RHIDevice& device, const BindGroupDesc& desc);
    };

} // namespace VKE::RHI
