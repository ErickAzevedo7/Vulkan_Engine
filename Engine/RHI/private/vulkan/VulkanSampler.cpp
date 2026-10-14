#include "VulkanSampler.h"
#include "VulkanDevice.h"
#include "VulkanUtils.h"

#include <algorithm>

namespace VKE::RHI {

    VulkanSampler::VulkanSampler(VulkanDevice& device, const SamplerDesc& desc) : m_Device(device) {
        f32 maxAnisotropy = std::min(desc.MaxAnisotropy, m_Device.GetLimits().MaxSamplerAnisotropy);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = ToVkFilter(desc.MagFilter);
        samplerInfo.minFilter = ToVkFilter(desc.MinFilter);
        samplerInfo.mipmapMode = ToVkMipmapMode(desc.MipFilter);
        samplerInfo.addressModeU = ToVkAddressMode(desc.AddressModeU);
        samplerInfo.addressModeV = ToVkAddressMode(desc.AddressModeV);
        samplerInfo.addressModeW = ToVkAddressMode(desc.AddressModeW);
        samplerInfo.anisotropyEnable = maxAnisotropy > 1.0f ? VK_TRUE : VK_FALSE;
        samplerInfo.maxAnisotropy = std::max(maxAnisotropy, 1.0f);
        samplerInfo.compareEnable = desc.CompareEnable ? VK_TRUE : VK_FALSE;
        samplerInfo.compareOp = ToVkCompareOp(desc.CompareOp);
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

        VK_CHECK(vkCreateSampler(m_Device.GetDevice(), &samplerInfo, nullptr, &m_Sampler));
        m_Device.SetDebugName(VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<u64>(m_Sampler), desc.DebugName);
    }

    VulkanSampler::~VulkanSampler() {
        if (m_Sampler) vkDestroySampler(m_Device.GetDevice(), m_Sampler, nullptr);
    }

    Iunique<RHISampler> RHISampler::Create(RHIDevice& device, const SamplerDesc& desc) {
        return MakeIunique<VulkanSampler>(static_cast<VulkanDevice&>(device), desc);
    }

} // namespace VKE::RHI
