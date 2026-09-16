#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>
#include <cstddef>

namespace VKE::RHI {

    class RHIDevice;

    // GPU buffer for vertex, index or uniform data.
    class RHIBuffer {
    public:
        virtual ~RHIBuffer() = default;

        // If `initialData` is given, desc.Size bytes are copied into the buffer.
        static Iunique<RHIBuffer> Create(RHIDevice& device, const BufferDesc& desc, const void* initialData = nullptr);

        virtual size_t GetSize() const = 0;

        // Copies `size` bytes into the buffer at `offset`. HostVisible buffers only.
        virtual void Write(const void* data, size_t size, size_t offset = 0) = 0;
    };

} // namespace VKE::RHI
