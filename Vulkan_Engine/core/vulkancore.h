#pragma once

#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <tiny_obj_loader.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "core/utils/Utils.h"
#include "mesh/MeshManager.h"

extern const int MAX_FRAMES_IN_FLIGHT;

extern const uint32_t WIDTH;
extern const uint32_t HEIGHT;

extern const std::string MODEL_PATH;
extern const std::string TEXTURE_PATH;

extern const std::vector<const char*> validationLayers;

extern const std::vector<const char*> deviceExtensions;

extern const bool enableValidationLayers;

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool isComplete();
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

struct UniformBufferObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

extern std::vector<Vertex> vertices;
extern std::vector<uint32_t> indices;

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger);

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks* pAllocator);

class VulkanCore {
 public:
  void initWindow();

  void initVulkan();

  void cleanup();

  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

  void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

  void updateUniformBuffer(uint32_t currentImage);

  void recreateSwapChain();

  VkFormat findDepthFormat();

  VkImageView createImageView(VkImage image,
                              VkFormat format,
                              VkImageAspectFlags aspectFlags,
                              uint32_t mipLevels);

  VkPipelineLayout getPipelineLayout();

  VkPipeline getPipeline();

  VkInstance getInstance();

  VkBuffer getVertexBuffer();

  VkBuffer getIndexBuffer();

  static VkPhysicalDevice getPhysicalDevice();

  static VkDevice getDevice();

  uint32_t getGraphicsQueueFamily();

  static VkQueue getGraphicsQueue();

  static VkQueue getPresentQueue();

  GLFWwindow* getWindow();

  VkFormat getSwapChainImageFormat();

  VkSampleCountFlagBits getmsaaSamples();

  bool getSwapChainRecreated();

  bool getFramebufferResized();

  void setFramebufferResized(bool value);

  void setSwapChainRecreated(bool value);

  std::vector<VkImageView> getSwapChainImageViews();

  VkImageView getDepthImageView();

  VkImageView getColorResolveImageView();

  VkExtent2D getSwapChainExtent();

  std::vector<VkSemaphore> getImageAvailableSemaphores();

  std::vector<VkSemaphore> getRenderFinishedSemaphores();

  std::vector<VkFence> getInFlightFences();

  uint32_t getCurrentFrame();

  void setCurrentFrame(uint32_t frame);

  VkSwapchainKHR getSwapChain();

  std::vector<VkCommandBuffer> getCommandBuffers();

  std::vector<VkDescriptorSet> getDescriptorSets();

  VkImageView getTextureImageView();

  VkSampler getTextureSampler();

  std::vector<void*> getUniformBuffersMapped();

  static VkCommandPool getCommandPool();

 private:
  GLFWwindow* window;
  VkInstance instance;
  VkDebugUtilsMessengerEXT debugMessenger;
  VkSurfaceKHR surface;
  static VkPhysicalDevice physicalDevice;
  static VkDevice device;
  static VkQueue graphicsQueue;
  static VkQueue presentQueue;
  VkSwapchainKHR swapChain;
  std::vector<VkImage> swapChainImages;
  VkFormat swapChainImageFormat;
  VkExtent2D swapChainExtent;
  std::vector<VkImageView> swapChainImageViews;
  VkRenderPass renderPass;
  VkDescriptorSetLayout descriptorSetLayout;
  VkPipelineLayout pipelineLayout;
  VkPipeline graphicsPipeline;
  std::vector<VkFramebuffer> swapChainFramebuffers;
  static VkCommandPool commandPool;
  std::vector<VkCommandBuffer> commandBuffers;
  std::vector<VkSemaphore> imageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;
  std::vector<VkFence> inFlightFences;
  uint32_t currentFrame;
  bool framebufferResized;
  bool swapChainRecreated;
  VkBuffer vertexBuffer;
  VkDeviceMemory vertexBufferMemory;
  VkBuffer indexBuffer;
  VkDeviceMemory indexBufferMemory;
  std::vector<VkBuffer> uniformBuffers;
  std::vector<VkDeviceMemory> uniformBuffersMemory;
  std::vector<void*> uniformBuffersMapped;
  VkDescriptorPool descriptorPool;
  std::vector<VkDescriptorSet> descriptorSets;
  uint32_t mipLevels;
  VkImage textureImage;
  VkDeviceMemory textureImageMemory;
  VkImageView textureImageView;
  VkSampler textureSampler;
  VkImage depthImage;
  VkDeviceMemory depthImageMemory;
  VkImageView depthImageView;
  VkSampleCountFlagBits msaaSamples;
  VkImage colorImage;
  VkDeviceMemory colorImageMemory;
  VkImageView colorImageView;

 private:
  static VKAPI_ATTR VkBool32 VKAPI_CALL
  debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                void* pUserData);

  static std::vector<char> readFile(const std::string& filename);

  static void framebufferResizeCallback(GLFWwindow* window,
                                        int width,
                                        int height);

  void createColorResources();

  VkSampleCountFlagBits getMaxUsableSampleCount();

  void loadModel();

  VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);

  void createDepthResources();

  void createTextureSampler();

  void createTextureImageView();

  void generateMipmaps(VkImage image,
                       VkFormat imageFormat,
                       int32_t texWidth,
                       int32_t texHeight,
                       uint32_t mipLevels);

  void createTextureImage();

  void createDescriptorSets();

  void createDescriptorPool();

  void createUniformBuffers();

  void createDescriptorSetLayout();

  void createSyncObjects();

  void createCommandBuffers();

  void createCommandPool();

  void createFramebuffers();

  void createRenderPass();

  void createGraphicsPipeline();

  VkShaderModule createShaderModule(const std::vector<char>& code);

  void createImageViews();

  void populateDebugMessengerCreateInfo(
      VkDebugUtilsMessengerCreateInfoEXT& createInfo);

  void setupDebugMessenger();

  void createSwapChain();

  void createSurface();

  void createLogicalDevice();

  void pickPhysicalDevice();

  int rateDeviceSuitability(VkPhysicalDevice device);

  bool checkDeviceExtensionSupport(VkPhysicalDevice device);

  void createInstance();

  bool checkValidationLayerSupport();

  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

  VkSurfaceFormatKHR chooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR>& availableFormats);

  VkPresentModeKHR chooseSwapPresentMode(
      const std::vector<VkPresentModeKHR>& availablePresentModes);

  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

  std::vector<const char*> getRequiredExtensions();

  void cleanupSwapChain();
};
