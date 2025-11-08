#include "skybox.h"
#include "Editor/EditorCamera.h"
#include "Editor/EditorCamera.h"
#include "Editor/EditorCamera.h"
#include "Editor/EditorCamera.h"
#include "Editor/EditorCamera.h"
#include "Editor/EditorCamera.h"
#include "Editor/EditorCamera.h"
#include "Editor/EditorCamera.h"
#include "Editor/EditorCamera.h"
#include "Editor/EditorCamera.h"
#include "Editor/EditorCamera.h"
#include "Editor/EditorCamera.h"
// initialize static members
VkImage Skybox::skyboxImage;
VkImageView Skybox::skyboxImageView;
VkDeviceMemory Skybox::skyboxImageMemory;
std::string Skybox::path = "textures/skybox/";
VkSampler Skybox::skyboxSampler;
std::vector<VkDescriptorSet> Skybox::skyboxDescriptorSet;
VkDescriptorSetLayout Skybox::skyboxDescriptorSetLayout;
VkDescriptorPool Skybox::skyboxDescriptorPool;
VkBuffer Skybox::skyboxVertexBuffer;
VkDeviceMemory Skybox::skyboxVertexBufferMemory;
VkPipeline Skybox::skyboxPipeline;
VkPipelineLayout Skybox::skyboxPipelineLayout;
std::vector<VkBuffer> Skybox::skyboxUniformBuffers;
std::vector<VkDeviceMemory> Skybox::skyboxUniformBuffersMemory;


void Skybox::init(VkCommandPool commandPool, VkRenderPass renderPass) {
  loadSkyboxTextures(commandPool);
  createSkyboxImageViews();
  createSkyboxSampler();
  createSkyboxUniformBuffers();
  createDescriptorSet();
  createSkyboxVertexBuffer(commandPool);
  createSkyboxPipeline(renderPass);
}

void Skybox::cleanup() {
  vkDestroySampler(VulkanCore::getDevice(), skyboxSampler, nullptr);
  vkDestroyImageView(VulkanCore::getDevice(), skyboxImageView, nullptr);
  vkDestroyImage(VulkanCore::getDevice(), skyboxImage, nullptr);
  vkFreeMemory(VulkanCore::getDevice(), skyboxImageMemory, nullptr);
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroyBuffer(VulkanCore::getDevice(), skyboxUniformBuffers[i], nullptr);
    vkFreeMemory(VulkanCore::getDevice(), skyboxUniformBuffersMemory[i],
                 nullptr);
  }
  vkDestroyDescriptorPool(VulkanCore::getDevice(), skyboxDescriptorPool,
                          nullptr);
  vkDestroyDescriptorSetLayout(VulkanCore::getDevice(),
                               skyboxDescriptorSetLayout, nullptr);
  vkDestroyBuffer(VulkanCore::getDevice(), skyboxVertexBuffer, nullptr);
  vkFreeMemory(VulkanCore::getDevice(), skyboxVertexBufferMemory, nullptr);
  vkDestroyPipeline(VulkanCore::getDevice(), skyboxPipeline, nullptr);
  vkDestroyPipelineLayout(VulkanCore::getDevice(), skyboxPipelineLayout,
                          nullptr);
}

std::vector<VkDescriptorSet> Skybox::getSkyboxDescriptorSet() {
  return skyboxDescriptorSet;
}

VkBuffer& Skybox::getSkyboxVertexBuffer() {
  return skyboxVertexBuffer;
}

VkPipeline Skybox::getSkyboxPipeline() {
  return skyboxPipeline;
}

VkPipelineLayout Skybox::getSkyboxPipelineLayout() {
  return skyboxPipelineLayout;
}

void Skybox::updateSkyboxUniformBuffer(uint32_t currentImage,
                                       const glm::mat4& view,
                                       const glm::mat4& proj) {
  SkyboxUniformBufferObject ubo{};
  ubo.view = view;
  ubo.proj = proj;

  void* data;
  vkMapMemory(VulkanCore::getDevice(), skyboxUniformBuffersMemory[currentImage],
              0, sizeof(ubo), 0, &data);
  memcpy(data, &ubo, sizeof(ubo));
  vkUnmapMemory(VulkanCore::getDevice(),
                skyboxUniformBuffersMemory[currentImage]);
}


void Skybox::createSkyboxUniformBuffers() {
  VkDeviceSize bufferSize = sizeof(SkyboxUniformBufferObject);
  skyboxUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  skyboxUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    Utils::createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        skyboxUniformBuffers[i], skyboxUniformBuffersMemory[i]);
  }
}


void Skybox::createSkyboxPipeline(VkRenderPass renderPass) {
		auto vertShaderCode = Utils::readFile("shaders/skybox.vert.spv");
    auto fragShaderCode = Utils::readFile("shaders/skybox.frag.spv");

    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;

	  Utils::createShaderModule(vertShaderCode, vertShaderModule);
    Utils::createShaderModule(fragShaderCode, fragShaderModule);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input (positions only)
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    // For skybox pipeline only:
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride =
        sizeof(glm::vec3);  // or whatever your skybox vertex is
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescription{};
    attributeDescription.binding = 0;
    attributeDescription.location = 0;
    attributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescription.offset = 0;

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    vertexInputInfo.pVertexAttributeDescriptions = &attributeDescription;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable =
        VK_TRUE;  // enable sample shading in the pipeline
    multisampling.minSampleShading =
        .2f;  // min fraction for sample shading; closer to one is smoother
    multisampling.rasterizationSamples =  VulkanCore::getmsaaSamples();

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(float) * 3 + sizeof(int);

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount =
        static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &skyboxDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(VulkanCore::getDevice(), &pipelineLayoutInfo, nullptr,
                               &skyboxPipelineLayout) != VK_SUCCESS) {
      throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
    pipelineInfo.basePipelineIndex = 0;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.layout = skyboxPipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(VulkanCore::getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo,
                                  nullptr, &skyboxPipeline) != VK_SUCCESS) {
      throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(VulkanCore::getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(VulkanCore::getDevice(), vertShaderModule, nullptr);
}

void Skybox::createSkyboxVertexBuffer(VkCommandPool commandPool) {
  VkDeviceSize bufferSize = sizeof(skyboxVertices[0]) * skyboxVertices.size();
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  Utils::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      stagingBuffer, stagingBufferMemory);
  void* data;
  vkMapMemory(VulkanCore::getDevice(), stagingBufferMemory, 0, bufferSize, 0,
              &data);
  memcpy(data, skyboxVertices.data(), (size_t)bufferSize);
  vkUnmapMemory(VulkanCore::getDevice(), stagingBufferMemory);
  Utils::createBuffer(
      bufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, skyboxVertexBuffer,
      skyboxVertexBufferMemory);
  Utils::copyBuffer(commandPool, stagingBuffer, skyboxVertexBuffer, bufferSize);
  vkDestroyBuffer(VulkanCore::getDevice(), stagingBuffer, nullptr);
  vkFreeMemory(VulkanCore::getDevice(), stagingBufferMemory, nullptr);
}

void Skybox::createDescriptorSet() {
  skyboxDescriptorSet.resize(MAX_FRAMES_IN_FLIGHT);

  VkDescriptorPoolSize poolSizes{};
  poolSizes.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSizes;
  poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  if (vkCreateDescriptorPool(VulkanCore::getDevice(), &poolInfo, nullptr,
                             &skyboxDescriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }

  std::array<VkDescriptorSetLayoutBinding, 2> samplerLayoutBinding{};
  samplerLayoutBinding[0].binding = 0;
  samplerLayoutBinding[0].descriptorType =
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  samplerLayoutBinding[0].descriptorCount = 1;
  samplerLayoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  samplerLayoutBinding[0].pImmutableSamplers = nullptr;

  samplerLayoutBinding[1].binding = 1;
  samplerLayoutBinding[1].descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerLayoutBinding[1].descriptorCount = 1;
  samplerLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  samplerLayoutBinding[1].pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 2;
  layoutInfo.pBindings = samplerLayoutBinding.data();

  if (vkCreateDescriptorSetLayout(VulkanCore::getDevice(), &layoutInfo, nullptr,
                                  &skyboxDescriptorSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }

  std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                             skyboxDescriptorSetLayout);

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = skyboxDescriptorPool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocInfo.pSetLayouts = layouts.data();

  if (vkAllocateDescriptorSets(VulkanCore::getDevice(), &allocInfo,
                               skyboxDescriptorSet.data()) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate descriptor sets!");
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = skyboxUniformBuffers[i];
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(SkyboxUniformBufferObject);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = skyboxImageView;
    imageInfo.sampler = skyboxSampler;

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = skyboxDescriptorSet[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType =
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;
    descriptorWrites[0].pImageInfo = nullptr;        // Optional
    descriptorWrites[0].pTexelBufferView = nullptr;  // Optional

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].pNext = nullptr;
    descriptorWrites[1].dstSet = skyboxDescriptorSet[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(VulkanCore::getDevice(),
                           static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);
  }
}

void Skybox::createSkyboxSampler() {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = 1.0f;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 0.0f;

  if (vkCreateSampler(VulkanCore::getDevice(), &samplerInfo, nullptr,
                      &skyboxSampler) != VK_SUCCESS) {
    throw std::runtime_error("failed to create cubemap sampler!");
  }
}

void Skybox::createSkyboxImage(VkCommandPool commandPool, uint32_t width, uint32_t height) {
  VkImageCreateInfo imageCreateCI{};
  imageCreateCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  imageCreateCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
  imageCreateCI.format = VK_FORMAT_R8G8B8A8_SRGB;
  imageCreateCI.extent = {width, height, 1};
  imageCreateCI.mipLevels = 1;
  imageCreateCI.arrayLayers = 6;
  imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateCI.usage =
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  imageCreateCI.tiling = VK_IMAGE_TILING_OPTIMAL;

  vkCreateImage(VulkanCore::getDevice(), &imageCreateCI, nullptr, &skyboxImage);

  VkMemoryRequirements memRequirements;
  VkMemoryAllocateInfo memAllocInfo{};
  memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  // VkDeviceMemory dstImageMemory;
  vkGetImageMemoryRequirements(VulkanCore::getDevice(), skyboxImage,
                               &memRequirements);
  memAllocInfo.allocationSize = memRequirements.size;
  // Memory must be host visible to copy from
  memAllocInfo.memoryTypeIndex = Utils::findMemoryType(
      memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  if (vkAllocateMemory(VulkanCore::getDevice(), &memAllocInfo, nullptr,
                       &skyboxImageMemory) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate image memory!");
  }
  vkBindImageMemory(VulkanCore::getDevice(), skyboxImage, skyboxImageMemory, 0);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = commandPool;
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
  imageMemoryBarrier.image = skyboxImage;
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

  vkQueueSubmit(VulkanCore::getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(VulkanCore::getGraphicsQueue());

  vkFreeCommandBuffers(VulkanCore::getDevice(), commandPool, 1, &copyCmd);
}

void Skybox::createSkyboxImageViews() {
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = skyboxImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
  viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 6;

  if (vkCreateImageView(VulkanCore::getDevice(), &viewInfo, nullptr,
                        &skyboxImageView) != VK_SUCCESS) {
    throw std::runtime_error("failed to create texture image view!");
  }
}

void Skybox::loadSkyboxTextures(VkCommandPool commandPool) {
  const std::array<std::string, 6> faces = {
      path + "right.bmp",  path + "left.bmp",  path + "top.bmp",
      path + "bottom.bmp", path + "front.bmp", path + "back.bmp"};
  int texWidth, texHeight, texChannels;
  std::vector<stbi_uc*> facePixels;
  stbi_set_flip_vertically_on_load(false);
  VkDeviceSize layerSize = 0;

  // Load each face of the cubemap
  for (const auto& face : faces) {
    stbi_uc* pixels = stbi_load(face.c_str(), &texWidth, &texHeight,
                                &texChannels, STBI_rgb_alpha);
    if (!pixels) {
      throw std::runtime_error("failed to load texture image!");
    }

    layerSize = texWidth * texHeight * 4;

    facePixels.push_back(pixels);
  }

  VkDeviceSize imageSize = layerSize * 6;

  createSkyboxImage(commandPool, texWidth, texHeight);

  // Create staging buffer
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  Utils::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(VulkanCore::getDevice(), stagingBufferMemory, 0, imageSize, 0,
              &data);
  for (size_t i = 0; i < faces.size(); i++) {
    memcpy(static_cast<char*>(data) + layerSize * i, facePixels[i],
           static_cast<size_t>(layerSize));
    stbi_image_free(facePixels[i]);
  }
  vkUnmapMemory(VulkanCore::getDevice(), stagingBufferMemory);

  Utils::transitionImageLayout(
      skyboxImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, commandPool, 6);
  Utils::copyBufferToImage(stagingBuffer, skyboxImage,
                           static_cast<uint32_t>(texWidth),
                           static_cast<uint32_t>(texHeight), commandPool);

  // Copy buffer to cubemap image, one layer per face
  VkCommandBuffer commandBuffer = Utils::beginSingleTimeCommands(commandPool);

  std::vector<VkBufferImageCopy> bufferCopyRegions;
  for (uint32_t face = 0; face < faces.size(); face++) {
    VkBufferImageCopy region{};
    region.bufferOffset = layerSize * face;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = face;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(texWidth),
                          static_cast<uint32_t>(texHeight), 1};
    bufferCopyRegions.push_back(region);
  }

  vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, skyboxImage,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         static_cast<uint32_t>(bufferCopyRegions.size()),
                         bufferCopyRegions.data());

  Utils::endSingleTimeCommands(commandPool, commandBuffer);

  Utils::transitionImageLayout(skyboxImage, VK_FORMAT_B8G8R8A8_SRGB,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                               commandPool, 6);

  vkDestroyBuffer(VulkanCore::getDevice(), stagingBuffer, nullptr);
  vkFreeMemory(VulkanCore::getDevice(), stagingBufferMemory, nullptr);
}
