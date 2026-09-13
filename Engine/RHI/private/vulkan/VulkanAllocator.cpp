#define VMA_IMPLEMENTATION
#include "VulkanAllocator.h"

namespace VKE::RHI {

    VulkanAllocator::VulkanAllocator(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
                                      u32 vulkanApiVersion) {
        VmaAllocatorCreateInfo createInfo{};
        createInfo.instance = instance;
        createInfo.physicalDevice = physicalDevice;
        createInfo.device = device;
        createInfo.vulkanApiVersion = vulkanApiVersion;

        vmaCreateAllocator(&createInfo, &m_Allocator);
    }

    VulkanAllocator::~VulkanAllocator() {
        if (m_Allocator) {
            vmaDestroyAllocator(m_Allocator);
        }
    }

} // namespace VKE::RHI
