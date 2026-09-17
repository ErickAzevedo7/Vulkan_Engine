#pragma once

#include "VulkanDescriptorAllocator.h"
#include <rhi/RHIBindGroup.h>
#include <vector>
#include <vulkan/vulkan.h>

namespace VKE::RHI {

    class VulkanDevice;

    class VulkanBindGroupLayout : public RHIBindGroupLayout {
    public:
        VulkanBindGroupLayout(VulkanDevice& device, const BindGroupLayoutDesc& desc);
        ~VulkanBindGroupLayout() override;

        VkDescriptorSetLayout GetHandle() const { return m_Layout; }

        // Sorted by binding.
        const std::vector<BindingSlot>& GetSlots() const { return m_Slots; }
        u32 GetDynamicOffsetCount() const { return m_DynamicOffsetCount; }
        u32 GetBufferSlotCount() const { return m_BufferSlotCount; }
        u32 GetImageSlotCount() const { return m_ImageSlotCount; }

    private:
        VulkanDevice& m_Device;
        VkDescriptorSetLayout m_Layout = VK_NULL_HANDLE;
        std::vector<BindingSlot> m_Slots;
        u32 m_DynamicOffsetCount = 0;
        u32 m_BufferSlotCount = 0;
        u32 m_ImageSlotCount = 0;
    };

    class VulkanBindGroup : public RHIBindGroup {
    public:
        VulkanBindGroup(VulkanDevice& device, const BindGroupDesc& desc);
        ~VulkanBindGroup() override;

        VkDescriptorSet GetHandle() const { return m_Allocation.Set; }
        u32 GetDynamicOffsetCount() const { return m_DynamicOffsetCount; }

    private:
        VulkanDevice& m_Device;
        VulkanDescriptorAllocator::Allocation m_Allocation;
        u32 m_DynamicOffsetCount = 0;
    };

} // namespace VKE::RHI
