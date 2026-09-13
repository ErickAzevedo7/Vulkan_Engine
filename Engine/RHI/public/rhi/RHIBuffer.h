#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>
#include <cstddef>

namespace VKE::RHI {

    class RHIDevice;

    // GPU buffer (vertex/index/uniform). Only the Vertex path is exercised
    // so far (the sandbox triangle's vertex buffer); Index/Uniform are part
    // of the interface already so adding them later isn't a signature change.
    class RHIBuffer {
    public:
        virtual ~RHIBuffer() = default;

        // Creates a buffer of `size` bytes. If `data` is non-null, it is
        // copied in immediately (uploaded once, not intended for per-frame
        // updates). Allocated through the device's VMA allocator.
        static Iunique<RHIBuffer> Create(RHIDevice& device, const void* data, size_t size, BufferUsage usage);

        virtual size_t GetSize() const = 0;
    };

} // namespace VKE::RHI
