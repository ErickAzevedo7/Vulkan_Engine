#pragma once

#include <core/Types.h>
#include <vector>
#include <vulkan/vulkan.h>

namespace VKE::RHI {

    // Allocates descriptor sets, adding pools as the existing ones fill up.
    class VulkanDescriptorAllocator {
    public:
        struct Allocation {
            VkDescriptorSet Set = VK_NULL_HANDLE;
            VkDescriptorPool Pool = VK_NULL_HANDLE;
        };

        explicit VulkanDescriptorAllocator(VkDevice device);
        ~VulkanDescriptorAllocator();

        VulkanDescriptorAllocator(const VulkanDescriptorAllocator&) = delete;
        VulkanDescriptorAllocator& operator=(const VulkanDescriptorAllocator&) = delete;

        Allocation Allocate(VkDescriptorSetLayout layout);
        void Free(const Allocation& allocation);

    private:
        VkDescriptorPool CreatePool();

        VkDevice m_Device = VK_NULL_HANDLE;
        std::vector<VkDescriptorPool> m_Pools;
    };

} // namespace VKE::RHI
