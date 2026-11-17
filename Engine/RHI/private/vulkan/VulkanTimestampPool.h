#pragma once

#include <rhi/RHITimestampPool.h>

#include <vulkan/vulkan.h>

namespace VKE {

class VulkanDevice;

class VulkanTimestampPool : public RHITimestampPool {
public:
	VulkanTimestampPool(VulkanDevice &device, u32 count);
	~VulkanTimestampPool() override;

	VulkanTimestampPool(const VulkanTimestampPool &) = delete;
	VulkanTimestampPool &operator=(const VulkanTimestampPool &) = delete;

	u32 GetCount() const override { return m_Count; }
	bool Read(u32 first, u32 count, u64 *outTicks) override;

	VkQueryPool GetHandle() const { return m_Pool; }

private:
	VulkanDevice &m_Device;
	VkQueryPool m_Pool = VK_NULL_HANDLE;
	u32 m_Count = 0;
};

} // namespace VKE
