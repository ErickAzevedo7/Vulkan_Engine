#pragma once

#include <rhi/RHITexture.h>
#include <unordered_map>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace VKE::RHI {

    class VulkanDevice;

    class VulkanTexture : public RHITexture {
    public:
        VulkanTexture(VulkanDevice& device, const TextureDesc& desc, const void* initialData);
        ~VulkanTexture() override;

        u32 GetWidth() const override { return m_Width; }
        u32 GetHeight() const override { return m_Height; }
        Format GetFormat() const override { return m_Format; }
        u32 GetMipLevels() const override { return m_MipLevels; }
        u32 GetArrayLayers() const override { return m_ArrayLayers; }
        void* GetView() const override { return m_View; }
        void* GetAttachmentView(u32 mipLevel, u32 layer) override;

        VkImage GetImage() const { return m_Image; }
        VkImageView GetImageView() const { return m_View; }

    private:
        VkImageView CreateView(VkImageViewType type, u32 baseMip, u32 mipCount, u32 baseLayer, u32 layerCount);
        void Upload(const void* data, bool generateMips);

        VulkanDevice& m_Device;
        VkImage m_Image = VK_NULL_HANDLE;
        VmaAllocation m_Allocation = VK_NULL_HANDLE;
        VkImageView m_View = VK_NULL_HANDLE;

        // One 2D view per mip level and layer rendered into, keyed by both.
        std::unordered_map<u32, VkImageView> m_AttachmentViews;

        u32 m_Width = 0;
        u32 m_Height = 0;
        u32 m_MipLevels = 1;
        u32 m_ArrayLayers = 1;
        Format m_Format = Format::Undefined;
        VkFormat m_VkFormat = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags m_Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    };

} // namespace VKE::RHI
