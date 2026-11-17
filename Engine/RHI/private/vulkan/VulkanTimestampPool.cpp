#include "VulkanTimestampPool.h"

#include "VulkanDevice.h"
#include "VulkanUtils.h"

#include <core/Assert.h>

namespace VKE {

VulkanTimestampPool::VulkanTimestampPool(VulkanDevice &device, u32 count) : m_Device(device), m_Count(count) {
	VKE_CORE_ASSERT(m_Device.GetLimits().TimestampValidBits != 0, "The device can't write timestamps");

	VkQueryPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	poolInfo.queryCount = count;
	VK_CHECK(vkCreateQueryPool(m_Device.GetDevice(), &poolInfo, nullptr, &m_Pool));
	m_Device.SetDebugName(VK_OBJECT_TYPE_QUERY_POOL, reinterpret_cast<u64>(m_Pool), "Timestamps");
}

VulkanTimestampPool::~VulkanTimestampPool() {
	if (m_Pool) {
		vkDestroyQueryPool(m_Device.GetDevice(), m_Pool, nullptr);
	}
}

bool VulkanTimestampPool::Read(u32 first, u32 count, u64 *outTicks) {
	VKE_CORE_ASSERT(first + count <= m_Count, "Timestamps read past the end of the pool");
	if (count == 0) {
		return true;
	}
	// Without VK_QUERY_RESULT_WAIT_BIT this returns VK_NOT_READY when any slot isn't available.
	VkResult result = vkGetQueryPoolResults(m_Device.GetDevice(), m_Pool, first, count, count * sizeof(u64), outTicks,
			sizeof(u64), VK_QUERY_RESULT_64_BIT);
	return result == VK_SUCCESS;
}

Iunique<RHITimestampPool> RHITimestampPool::Create(RHIDevice &device, u32 count) {
	return MakeIunique<VulkanTimestampPool>(static_cast<VulkanDevice &>(device), count);
}

} // namespace VKE
