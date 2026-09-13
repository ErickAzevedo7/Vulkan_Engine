#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanUtils.h"

#include <cstring>

namespace VKE::RHI {

    namespace {
        VkBufferUsageFlags ToVkBufferUsage(BufferUsage usage) {
            switch (usage) {
                case BufferUsage::Vertex: return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
                case BufferUsage::Index: return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
                case BufferUsage::Uniform: return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                default: return 0;
            }
        }
    } // namespace

    VulkanBuffer::VulkanBuffer(VulkanDevice& device, const void* data, size_t size, BufferUsage usage)
        : m_Device(device), m_Size(size) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = ToVkBufferUsage(usage);
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // Host-visible + persistently mapped: fine for small, load-once
        // buffers (e.g. the sandbox triangle's vertex data). A staging-buffer
        // upload path for large GPU-only meshes isn't implemented yet.
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo resultInfo{};
        VK_CHECK(vmaCreateBuffer(m_Device.GetAllocator(), &bufferInfo, &allocInfo, &m_Buffer, &m_Allocation, &resultInfo));

        if (data) {
            std::memcpy(resultInfo.pMappedData, data, size);
        }
    }

    VulkanBuffer::~VulkanBuffer() {
        if (m_Buffer) {
            vmaDestroyBuffer(m_Device.GetAllocator(), m_Buffer, m_Allocation);
        }
    }

    Iunique<RHIBuffer> RHIBuffer::Create(RHIDevice& device, const void* data, size_t size, BufferUsage usage) {
        return MakeIunique<VulkanBuffer>(static_cast<VulkanDevice&>(device), data, size, usage);
    }

} // namespace VKE::RHI
