#include "VulkanDescriptorAllocator.h"
#include "VulkanUtils.h"

namespace VKE::RHI {

    namespace {
        constexpr u32 kSetsPerPool = 256;

        const VkDescriptorPoolSize kPoolSizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kSetsPerPool},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, kSetsPerPool / 4},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kSetsPerPool},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kSetsPerPool * 6},
        };
    } // namespace

    VulkanDescriptorAllocator::VulkanDescriptorAllocator(VkDevice device) : m_Device(device) {
    }

    VulkanDescriptorAllocator::~VulkanDescriptorAllocator() {
        for (VkDescriptorPool pool : m_Pools) {
            vkDestroyDescriptorPool(m_Device, pool, nullptr);
        }
    }

    VulkanDescriptorAllocator::Allocation VulkanDescriptorAllocator::Allocate(VkDescriptorSetLayout layout) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        // Newest pools are most likely to have room.
        for (auto it = m_Pools.rbegin(); it != m_Pools.rend(); ++it) {
            allocInfo.descriptorPool = *it;
            VkDescriptorSet set = VK_NULL_HANDLE;
            VkResult result = vkAllocateDescriptorSets(m_Device, &allocInfo, &set);
            if (result == VK_SUCCESS) {
                return {set, *it};
            }
            if (result != VK_ERROR_OUT_OF_POOL_MEMORY && result != VK_ERROR_FRAGMENTED_POOL) {
                VK_CHECK(result);
                return {};
            }
        }

        m_Pools.push_back(CreatePool());
        allocInfo.descriptorPool = m_Pools.back();
        VkDescriptorSet set = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateDescriptorSets(m_Device, &allocInfo, &set));
        return {set, m_Pools.back()};
    }

    void VulkanDescriptorAllocator::Free(const Allocation& allocation) {
        if (allocation.Set) {
            vkFreeDescriptorSets(m_Device, allocation.Pool, 1, &allocation.Set);
        }
    }

    VkDescriptorPool VulkanDescriptorAllocator::CreatePool() {
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = kSetsPerPool;
        poolInfo.poolSizeCount = static_cast<u32>(std::size(kPoolSizes));
        poolInfo.pPoolSizes = kPoolSizes;

        VkDescriptorPool pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &pool));
        return pool;
    }

} // namespace VKE::RHI
