#pragma once

// Thin VMA wrapper — VulkanBuffer/VulkanTexture allocate through this
// instead of raw vkAllocateMemory/vkBindBufferMemory (one allocation per
// resource), which is what the legacy engine does in
// Vulkan_Engine/Vulkan_Engine/Engine/Core/utils/Utils.cpp.

#include <core/Types.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace VKE::RHI {

    class VulkanAllocator {
    public:
        VulkanAllocator(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, u32 vulkanApiVersion);
        ~VulkanAllocator();

        VulkanAllocator(const VulkanAllocator&) = delete;
        VulkanAllocator& operator=(const VulkanAllocator&) = delete;

        VmaAllocator GetHandle() const { return m_Allocator; }

    private:
        VmaAllocator m_Allocator = VK_NULL_HANDLE;
    };

} // namespace VKE::RHI
