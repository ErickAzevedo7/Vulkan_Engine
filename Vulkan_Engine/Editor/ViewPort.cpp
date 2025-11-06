#include "ViewPort.h"

void ViewPort::init(VulkanCore* core, VkExtent2D viewportExtent) {
  engineCore = core;
  this->viewportExtent = viewportExtent;
	
  VkCommandPoolCreateInfo commandPoolCreateInfo{};
  commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolCreateInfo.queueFamilyIndex = engineCore->getGraphicsQueueFamily();
  commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  if (vkCreateCommandPool(VulkanCore::getDevice(), &commandPoolCreateInfo,
                          nullptr, &m_ViewportCommandPool) != VK_SUCCESS) {
    throw std::runtime_error("Could not create graphics command pool");
  }

  createViewportImage();
  createViewportImageViews();

  m_ViewportCommandBuffers.resize(m_ViewportImageViews.size());

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = m_ViewportCommandPool;
  allocInfo.commandBufferCount = (uint32_t)m_ViewportCommandBuffers.size();

  if (vkAllocateCommandBuffers(VulkanCore::getDevice(), &allocInfo,
                               m_ViewportCommandBuffers.data()) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
  }

  createViewportRenderPass();

  createViewportFramebuffers();
}

void ViewPort::cleanupFramebuffers() {
  for (auto framebuffer : m_ViewportFramebuffers) {
    vkDestroyFramebuffer(VulkanCore::getDevice(), framebuffer, nullptr);
  }

  for (auto imageView : m_ViewportImageViews) {
    vkDestroyImageView(VulkanCore::getDevice(), imageView, nullptr);
  }

  for (auto image : m_ViewportImages) {
    vkDestroyImage(VulkanCore::getDevice(), image, nullptr);
  }

  for (auto memory : m_DstImageMemory) {
    vkFreeMemory(VulkanCore::getDevice(), memory, nullptr);
  }
}

void ViewPort::recreateViewport(VkExtent2D viewportExtent) {
  this->viewportExtent = viewportExtent;
  createViewportImage();
  createViewportImageViews();
  createViewportFramebuffers();
}

void ViewPort::cleanup() {

  for (auto framebuffer : m_ViewportFramebuffers) {
    vkDestroyFramebuffer(VulkanCore::getDevice(), framebuffer, nullptr);
  }

  for (auto imageView : m_ViewportImageViews) {
    vkDestroyImageView(VulkanCore::getDevice(), imageView, nullptr);
  }

  for (auto image : m_ViewportImages) {
    vkDestroyImage(VulkanCore::getDevice(), image, nullptr);
  }

  for (auto memory : m_DstImageMemory) {
    vkFreeMemory(VulkanCore::getDevice(), memory, nullptr);
  }

  vkDestroyCommandPool(VulkanCore::getDevice(), m_ViewportCommandPool, nullptr);

  vkDestroyRenderPass(VulkanCore::getDevice(), m_ViewportRenderPass, nullptr);
}

void ViewPort::createViewportImage() {
  m_ViewportImages.resize(engineCore->getSwapChainImageViews().size());
  m_DstImageMemory.resize(engineCore->getSwapChainImageViews().size());

  for (uint32_t i = 0; i < engineCore->getSwapChainImageViews().size(); i++) {
    // Create the linear tiled destination image to copy to and to read the
    // memory from
    VkImageCreateInfo imageCreateCI{};
    imageCreateCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
    // Note that vkCmdBlitImage (if supported) will also do format conversions
    // if the swapchain color format would differ
    imageCreateCI.format = VK_FORMAT_B8G8R8A8_SRGB;
    imageCreateCI.extent.width = viewportExtent.width;
    imageCreateCI.extent.height = viewportExtent.height;
    imageCreateCI.extent.depth = 1;
    imageCreateCI.arrayLayers = 1;
    imageCreateCI.mipLevels = 1;
    imageCreateCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateCI.usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    // Create the image
    // VkImage dstImage;
    vkCreateImage(VulkanCore::getDevice(), &imageCreateCI, nullptr,
                  &m_ViewportImages[i]);
    // Create memory to back up the image
    VkMemoryRequirements memRequirements;
    VkMemoryAllocateInfo memAllocInfo{};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    // VkDeviceMemory dstImageMemory;
    vkGetImageMemoryRequirements(VulkanCore::getDevice(), m_ViewportImages[i],
                                 &memRequirements);
    memAllocInfo.allocationSize = memRequirements.size;
    // Memory must be host visible to copy from
    memAllocInfo.memoryTypeIndex = Utils::findMemoryType(
        memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(VulkanCore::getDevice(), &memAllocInfo, nullptr,
                         &m_DstImageMemory[i]) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate image memory!");
    }
    vkBindImageMemory(VulkanCore::getDevice(), m_ViewportImages[i],
                      m_DstImageMemory[i], 0);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_ViewportCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer copyCmd;
    vkAllocateCommandBuffers(VulkanCore::getDevice(), &allocInfo, &copyCmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(copyCmd, &beginInfo);

    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageMemoryBarrier.image = m_ViewportImages[i];
    imageMemoryBarrier.subresourceRange =
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &imageMemoryBarrier);

    vkEndCommandBuffer(copyCmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &copyCmd;

    vkQueueSubmit(VulkanCore::getGraphicsQueue(), 1, &submitInfo,
                  VK_NULL_HANDLE);
    vkQueueWaitIdle(VulkanCore::getGraphicsQueue());

    vkFreeCommandBuffers(VulkanCore::getDevice(), m_ViewportCommandPool, 1,
                         &copyCmd);
  }
}

void ViewPort::createViewportImageViews() {
  m_ViewportImageViews.resize(m_ViewportImages.size());
  for (uint32_t i = 0; i < m_ViewportImages.size(); i++) {
    m_ViewportImageViews[i] =
        Utils::createImageView(m_ViewportImages[i], VK_FORMAT_B8G8R8A8_SRGB,
                               VK_IMAGE_ASPECT_COLOR_BIT, 1);
  }
}

void ViewPort::createViewportRenderPass() {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = engineCore->getSwapChainImageFormat();
  colorAttachment.samples = engineCore->getmsaaSamples();
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depthAttachment{};
  depthAttachment.format = engineCore->findDepthFormat();
  depthAttachment.samples = engineCore->getmsaaSamples();
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription colorAttachmentResolve{};
  colorAttachmentResolve.format = engineCore->getSwapChainImageFormat();
  colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colorAttachmentResolveRef{};
  colorAttachmentResolveRef.attachment = 2;
  colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;
  subpass.pResolveAttachments = &colorAttachmentResolveRef;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  std::array<VkAttachmentDescription, 3> attachments = {
      colorAttachment, depthAttachment, colorAttachmentResolve};
  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  if (vkCreateRenderPass(VulkanCore::getDevice(), &renderPassInfo, nullptr,
                         &m_ViewportRenderPass) != VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }
}

void ViewPort::createViewportFramebuffers() {
  m_ViewportFramebuffers.resize(m_ViewportImageViews.size());

  for (size_t i = 0; i < m_ViewportImageViews.size(); i++) {
    std::array<VkImageView, 3> attachments = {
        engineCore->getColorResolveImageView(),
        engineCore->getDepthImageView(),
        m_ViewportImageViews[i],
    };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_ViewportRenderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = viewportExtent.width;
    framebufferInfo.height = viewportExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(VulkanCore::getDevice(), &framebufferInfo, nullptr,
                            &m_ViewportFramebuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("failed to create framebuffer!");
    }
  }
}

void ViewPort::recordViewportCommandBuffer(VkCommandBuffer commandBuffer,
                                           uint32_t imageIndex) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  // beginInfo.flags = 0;
  // // Optional beginInfo.pInheritanceInfo = nullptr; // Optional

  if (vkBeginCommandBuffer(
          m_ViewportCommandBuffers[VulkanCore::getCurrentFrame()],
          &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = m_ViewportRenderPass;
  renderPassInfo.framebuffer = m_ViewportFramebuffers[imageIndex];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = viewportExtent;

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  clearValues[1].depthStencil = {1.0f, 0};

  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(m_ViewportCommandBuffers[VulkanCore::getCurrentFrame()],
                       &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)viewportExtent.width;
  viewport.height = (float)viewportExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = viewportExtent;
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  SceneRenderer::renderScene(commandBuffer, engineCore->getPipeline(),
                             engineCore->getPipelineLayout(), imageIndex);

  vkCmdEndRenderPass(commandBuffer);

  if (vkEndCommandBuffer(
          m_ViewportCommandBuffers[VulkanCore::getCurrentFrame()]) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }
}

void ViewPort::createViewportCommandBuffers() {
  m_ViewportCommandBuffers.resize(m_ViewportImageViews.size());

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_ViewportCommandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount =
      static_cast<uint32_t>(m_ViewportCommandBuffers.size());

  if (vkAllocateCommandBuffers(VulkanCore::getDevice(), &allocInfo,
                               m_ViewportCommandBuffers.data()) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate viewport command buffers!");
  }
}
