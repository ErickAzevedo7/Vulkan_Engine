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

    // A binding slot declared by a layout: its number, what it holds and which stages read it.
    struct BindingSlot {
        u32 Binding = 0;
        BindingType Type = BindingType::UniformBuffer;
        ShaderStageFlags Stages = ShaderStageFlags::AllGraphics;
    };

    struct BindGroupLayoutDesc {
        std::vector<BindingSlot> Slots;
        const char* DebugName = nullptr;
    };

    // The shape of a bind group: which slots exist. Shared by every bind group
    // created from it, and used by pipelines to accept those groups.
    class RHIBindGroupLayout {
    public:
        virtual ~RHIBindGroupLayout() = default;

        static Iunique<RHIBindGroupLayout> Create(RHIDevice& device, const BindGroupLayoutDesc& desc);
    };

    // The resource placed in one slot of a bind group.
    struct BoundResource {
        u32 Binding = 0;

        // Uniform and storage buffers. Size 0 binds from Offset to the end of the buffer.
        RHIBuffer* Buffer = nullptr;
        size_t Offset = 0;
        size_t Size = 0;

        // Combined image samplers.
        RHITexture* Texture = nullptr;
        RHISampler* Sampler = nullptr;
    };

    struct BindGroupDesc {
        RHIBindGroupLayout* Layout = nullptr;
        // One resource per layout slot.
        std::vector<BoundResource> Resources;
        const char* DebugName = nullptr;
    };

    // The contents of a bind group: the resources filling a layout's slots.
    // Immutable: create a new one to change its contents.
    class RHIBindGroup {
    public:
        virtual ~RHIBindGroup() = default;

        static Iunique<RHIBindGroup> Create(RHIDevice& device, const BindGroupDesc& desc);
    };

} // namespace VKE::RHI
