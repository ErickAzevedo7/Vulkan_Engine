#pragma once

#include <rhi/RHIBuffer.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace VKE::RHI {

    class VulkanDevice;

    class VulkanBuffer : public RHIBuffer {
    public:
        VulkanBuffer(VulkanDevice& device, const void* data, size_t size, BufferUsage usage);
        ~VulkanBuffer() override;

        size_t GetSize() const override { return m_Size; }
        VkBuffer GetHandle() const { return m_Buffer; }

    private:
        VulkanDevice& m_Device;
        VkBuffer m_Buffer = VK_NULL_HANDLE;
        VmaAllocation m_Allocation = VK_NULL_HANDLE;
        size_t m_Size = 0;
    };

} // namespace VKE::RHI
