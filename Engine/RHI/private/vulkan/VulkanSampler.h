#pragma once

#include <rhi/RHISampler.h>
#include <vulkan/vulkan.h>

namespace VKE::RHI {

    class VulkanDevice;

    class VulkanSampler : public RHISampler {
    public:
        VulkanSampler(VulkanDevice& device, const SamplerDesc& desc);
        ~VulkanSampler() override;

        VkSampler GetHandle() const { return m_Sampler; }

    private:
        VulkanDevice& m_Device;
        VkSampler m_Sampler = VK_NULL_HANDLE;
    };

} // namespace VKE::RHI
