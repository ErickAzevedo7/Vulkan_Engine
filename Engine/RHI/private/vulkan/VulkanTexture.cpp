#include "VulkanTexture.h"
#include "VulkanDevice.h"
#include "VulkanUtils.h"

#include <algorithm>
#include <cstring>

namespace VKE::RHI {

    namespace {
        VkImageUsageFlags ToVkImageUsage(TextureUsage usage) {
            VkImageUsageFlags result = 0;
            if (HasFlag(usage, TextureUsage::Sampled)) result |= VK_IMAGE_USAGE_SAMPLED_BIT;
            if (HasFlag(usage, TextureUsage::ColorAttachment)) result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            if (HasFlag(usage, TextureUsage::DepthAttachment)) result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            return result;
        }

        u32 CalculateMipLevels(u32 width, u32 height) {
            u32 levels = 1;
            for (u32 size = std::max(width, height); size > 1; size /= 2) {
                ++levels;
            }
            return levels;
        }

        struct LayoutState {
            VkAccessFlags Access;
            VkPipelineStageFlags Stage;
        };

        LayoutState GetLayoutState(VkImageLayout layout) {
            switch (layout) {
                case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return {VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
                case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return {VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                    return {VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
                default: return {0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
            }
        }

        void TransitionLayout(VkCommandBuffer cmd, VkImage image, u32 baseMip, u32 mipCount, VkImageLayout from,
                              VkImageLayout to) {
            LayoutState src = GetLayoutState(from);
            LayoutState dst = GetLayoutState(to);

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = from;
            barrier.newLayout = to;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, baseMip, mipCount, 0, 1};
            barrier.srcAccessMask = src.Access;
            barrier.dstAccessMask = dst.Access;

            vkCmdPipelineBarrier(cmd, src.Stage, dst.Stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        }
    } // namespace

    VulkanTexture::VulkanTexture(VulkanDevice& device, const TextureDesc& desc, const void* initialData)
        : m_Device(device), m_Width(desc.Width), m_Height(desc.Height), m_Format(desc.Format) {
        VKE_CORE_ASSERT(desc.Width > 0 && desc.Height > 0, "Texture size must be greater than zero");
        VKE_CORE_ASSERT(desc.Usage != TextureUsage::None, "Texture usage must be set");

        VkFormat format = ToVkFormat(desc.Format);
        VkImageUsageFlags usage = ToVkImageUsage(desc.Usage);

        if (desc.GenerateMips && initialData) {
            constexpr VkFormatFeatureFlags kBlitFeatures = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT |
                                                           VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(m_Device.GetPhysicalDevice(), format, &formatProperties);

            if ((formatProperties.optimalTilingFeatures & kBlitFeatures) == kBlitFeatures) {
                m_MipLevels = CalculateMipLevels(desc.Width, desc.Height);
            } else {
                VKE_CORE_WARN("Texture '{}': format can't be filtered for mip generation, using a single level",
                              desc.DebugName ? desc.DebugName : "<unnamed>");
            }
        }

        if (initialData) usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (m_MipLevels > 1) usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = {desc.Width, desc.Height, 1};
        imageInfo.mipLevels = m_MipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        VK_CHECK(vmaCreateImage(m_Device.GetAllocator(), &imageInfo, &allocInfo, &m_Image, &m_Allocation, nullptr));
        m_Device.SetDebugName(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<u64>(m_Image), desc.DebugName);

        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        if (IsDepthFormat(desc.Format)) {
            aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (HasStencil(desc.Format)) aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_Image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.levelCount = m_MipLevels;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(m_Device.GetDevice(), &viewInfo, nullptr, &m_View));

        if (initialData) {
            Upload(initialData);
        }
    }

    VulkanTexture::~VulkanTexture() {
        if (m_View) vkDestroyImageView(m_Device.GetDevice(), m_View, nullptr);
        if (m_Image) vmaDestroyImage(m_Device.GetAllocator(), m_Image, m_Allocation);
    }

    void VulkanTexture::Upload(const void* data) {
        u32 bytesPerPixel = GetBytesPerPixel(m_Format);
        VKE_CORE_ASSERT(bytesPerPixel > 0, "Texture format can't be uploaded from pixel data");
        VkDeviceSize size = static_cast<VkDeviceSize>(m_Width) * m_Height * bytesPerPixel;

        VmaAllocator allocator = m_Device.GetAllocator();

        VkBufferCreateInfo stagingInfo{};
        stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingInfo.size = size;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkBuffer staging = VK_NULL_HANDLE;
        VmaAllocation stagingAllocation = VK_NULL_HANDLE;
        VmaAllocationInfo stagingResult{};
        VK_CHECK(vmaCreateBuffer(allocator, &stagingInfo, &stagingAllocInfo, &staging, &stagingAllocation, &stagingResult));

        std::memcpy(stagingResult.pMappedData, data, static_cast<size_t>(size));
        vmaFlushAllocation(allocator, stagingAllocation, 0, size);

        m_Device.ImmediateSubmit([&](VkCommandBuffer cmd) {
            TransitionLayout(cmd, m_Image, 0, m_MipLevels, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {m_Width, m_Height, 1};
            vkCmdCopyBufferToImage(cmd, staging, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

            // Each level is downsampled from the previous one, then made readable.
            i32 mipWidth = static_cast<i32>(m_Width);
            i32 mipHeight = static_cast<i32>(m_Height);
            for (u32 level = 1; level < m_MipLevels; ++level) {
                TransitionLayout(cmd, m_Image, level - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

                i32 nextWidth = std::max(mipWidth / 2, 1);
                i32 nextHeight = std::max(mipHeight / 2, 1);

                VkImageBlit blit{};
                blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
                blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, 1};
                blit.dstOffsets[1] = {nextWidth, nextHeight, 1};
                blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1};
                vkCmdBlitImage(cmd, m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_Image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

                TransitionLayout(cmd, m_Image, level - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                mipWidth = nextWidth;
                mipHeight = nextHeight;
            }

            TransitionLayout(cmd, m_Image, m_MipLevels - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });

        vmaDestroyBuffer(allocator, staging, stagingAllocation);
    }

    Iunique<RHITexture> RHITexture::Create(RHIDevice& device, const TextureDesc& desc, const void* initialData) {
        return MakeIunique<VulkanTexture>(static_cast<VulkanDevice&>(device), desc, initialData);
    }

} // namespace VKE::RHI
