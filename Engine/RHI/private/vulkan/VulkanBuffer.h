#pragma once

#include <rhi/RHIBuffer.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace VKE::RHI {

    class VulkanDevice;

    class VulkanBuffer : public RHIBuffer {
    public:
        VulkanBuffer(VulkanDevice& device, const BufferDesc& desc, const void* initialData);
        ~VulkanBuffer() override;

        size_t GetSize() const override { return m_Size; }
        void Write(const void* data, size_t size, size_t offset) override;
        void Read(void* data, size_t size, size_t offset) override;

        VkBuffer GetHandle() const { return m_Buffer; }

    private:
        void UploadThroughStaging(const void* data);

        VulkanDevice& m_Device;
        VkBuffer m_Buffer = VK_NULL_HANDLE;
        VmaAllocation m_Allocation = VK_NULL_HANDLE;
        void* m_Mapped = nullptr;
        size_t m_Size = 0;
        BufferMemory m_Memory = BufferMemory::GpuOnly;
    };

} // namespace VKE::RHI
