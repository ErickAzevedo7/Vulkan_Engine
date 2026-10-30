#include "VulkanBuffer.h"
#include "VulkanCommandBuffer.h"
#include "VulkanDevice.h"
#include "VulkanUtils.h"

#include <cstring>

namespace VKE::RHI {

    namespace {
        VkBufferUsageFlags ToVkBufferUsage(BufferUsage usage) {
            VkBufferUsageFlags result = 0;
            if (HasFlag(usage, BufferUsage::Vertex)) result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            if (HasFlag(usage, BufferUsage::Index)) result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            if (HasFlag(usage, BufferUsage::Uniform)) result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            if (HasFlag(usage, BufferUsage::Storage)) result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            return result;
        }
    } // namespace

    VulkanBuffer::VulkanBuffer(VulkanDevice& device, const BufferDesc& desc, const void* initialData)
        : m_Device(device), m_Size(desc.Size), m_Memory(desc.Memory) {
        VKE_CORE_ASSERT(desc.Size > 0, "Buffer size must be greater than zero");

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = desc.Size;
        bufferInfo.usage = ToVkBufferUsage(desc.Usage);
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        if (desc.Memory == BufferMemory::HostVisible) {
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        } else if (desc.Memory == BufferMemory::Readback) {
            bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        } else {
            bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        }

        VmaAllocationInfo resultInfo{};
        VK_CHECK(vmaCreateBuffer(m_Device.GetAllocator(), &bufferInfo, &allocInfo, &m_Buffer, &m_Allocation, &resultInfo));
        m_Device.SetDebugName(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<u64>(m_Buffer), desc.DebugName);

        if (desc.Memory == BufferMemory::Readback) {
            m_Mapped = resultInfo.pMappedData;
        } else if (desc.Memory == BufferMemory::HostVisible) {
            m_Mapped = resultInfo.pMappedData;
            if (initialData) {
                Write(initialData, desc.Size, 0);
            }
        } else if (initialData) {
            UploadThroughStaging(initialData);
        }
    }

    VulkanBuffer::~VulkanBuffer() {
        if (m_Buffer) {
            vmaDestroyBuffer(m_Device.GetAllocator(), m_Buffer, m_Allocation);
        }
    }

    void VulkanBuffer::Write(const void* data, size_t size, size_t offset) {
        VKE_CORE_ASSERT(m_Memory == BufferMemory::HostVisible, "RHIBuffer::Write requires a HostVisible buffer");
        VKE_CORE_ASSERT(offset + size <= m_Size, "RHIBuffer::Write out of range");

        std::memcpy(static_cast<Byte*>(m_Mapped) + offset, data, size);
        vmaFlushAllocation(m_Device.GetAllocator(), m_Allocation, offset, size);
    }

    void VulkanBuffer::Read(void* data, size_t size, size_t offset) {
        VKE_CORE_ASSERT(m_Memory == BufferMemory::Readback, "RHIBuffer::Read requires a Readback buffer");
        VKE_CORE_ASSERT(offset + size <= m_Size, "RHIBuffer::Read out of range");

        vmaInvalidateAllocation(m_Device.GetAllocator(), m_Allocation, offset, size);
        std::memcpy(data, static_cast<const Byte*>(m_Mapped) + offset, size);
    }

    void VulkanBuffer::UploadThroughStaging(const void* data) {
        VmaAllocator allocator = m_Device.GetAllocator();

        VkBufferCreateInfo stagingInfo{};
        stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingInfo.size = m_Size;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkBuffer staging = VK_NULL_HANDLE;
        VmaAllocation stagingAllocation = VK_NULL_HANDLE;
        VmaAllocationInfo stagingResult{};
        VK_CHECK(vmaCreateBuffer(allocator, &stagingInfo, &stagingAllocInfo, &staging, &stagingAllocation, &stagingResult));

        std::memcpy(stagingResult.pMappedData, data, m_Size);
        vmaFlushAllocation(allocator, stagingAllocation, 0, m_Size);

        m_Device.ExecuteImmediate([&](RHICommandBuffer& commandBuffer) {
            VkCommandBuffer cmd = static_cast<VulkanCommandBuffer&>(commandBuffer).GetHandle();
            VkBufferCopy copy{};
            copy.size = m_Size;
            vkCmdCopyBuffer(cmd, staging, m_Buffer, 1, &copy);
        });

        vmaDestroyBuffer(allocator, staging, stagingAllocation);
    }

    Iunique<RHIBuffer> RHIBuffer::Create(RHIDevice& device, const BufferDesc& desc, const void* initialData) {
        return MakeIunique<VulkanBuffer>(static_cast<VulkanDevice&>(device), desc, initialData);
    }

} // namespace VKE::RHI
