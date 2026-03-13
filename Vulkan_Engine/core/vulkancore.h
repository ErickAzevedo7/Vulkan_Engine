#pragma once

// External libraries - required for interface
#include <cstdint>
#include <GLFW/glfw3.h>
#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "vulkan/vk_platform.h"
#include "vulkan/vulkan_core.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"

// GLM (needed for UniformBufferObject in interface)
#include <glm/glm.hpp>

#include "events/EventBus.h"

// Forward declarations
struct Vertex;
class LightManager;
namespace Renderer {
class VulkanShadowMap;
} // namespace Renderer

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

// Per-camera, per-render-pass data. Two instances exist in the engine:
// one for the Editor camera, one for the Game/CameraComponent camera.
struct GlobalUBO {
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
	alignas(16) glm::mat4 lightSpaceMatrices[6];
	alignas(16) glm::vec4 lightPos_farPlane;
	alignas(16) glm::vec3 viewPos;
};

// Per-entity data written once per entity per frame.
struct PerObjectUBO {
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 normal;
};

// Push constant data layout (matches shader.frag)
struct PushConstantData {
	glm::vec3 pickColor;
	int usePickColor;
	glm::mat4 lightSpaceMatrix;
};

// Legacy alias so existing code that references UniformBufferObject still compiles.
// TODO: migrate callers to GlobalUBO / PerObjectUBO directly.
using UniformBufferObject = PerObjectUBO;

extern std::vector<Vertex> vertices;
extern std::vector<uint32_t> indices;

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
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
	void createGraphicsPipeline(VkDescriptorSetLayout lightLayout,
								VkDescriptorSetLayout matLayout,
								VkDescriptorSetLayout objLayout);
	void cleanup();

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

	void recreateSwapChain();

	VkFormat findDepthFormat();

	VkPipelineLayout getPipelineLayout();

	VkPipeline getPipeline();

	VkInstance getInstance();

	static VkPhysicalDevice getPhysicalDevice();

	static VkDevice getDevice();

	uint32_t getGraphicsQueueFamily();

	static VkQueue getGraphicsQueue();

	static VkQueue getPresentQueue();

	GLFWwindow* getWindow();

	VkFormat getSwapChainImageFormat();

	static VkSampleCountFlagBits getmsaaSamples();

	bool getSwapChainRecreated();

	bool getFramebufferResized();

	void setFramebufferResized(bool value);

	static VkDescriptorSetLayout getDescriptorSetLayout();

	VkDescriptorPool getGlobalDescriptorPool() const {
		return globalDescriptorPool;
	}

	void setSwapChainRecreated(bool value);

	static std::vector<VkImageView> getSwapChainImageViews();

	static VkExtent2D getSwapChainExtent();

	std::vector<VkSemaphore> getImageAvailableSemaphores();

	std::vector<VkSemaphore> getRenderFinishedSemaphores();

	std::vector<VkFence> getInFlightFences();

	static uint32_t getCurrentFrame();

	void setCurrentFrame(uint32_t frame);

	VkSwapchainKHR getSwapChain();

	std::vector<VkCommandBuffer> getCommandBuffers();

	VkSampler getTextureSampler();

	static VkCommandPool getCommandPool();

	// --- Global UBO (per-camera) accessors ---
	static std::vector<VkBuffer>& getEditorGlobalBuffers();
	static std::vector<void*>& getEditorGlobalBuffersMapped();
	static std::vector<VkBuffer>& getGameGlobalBuffers();
	static std::vector<void*>& getGameGlobalBuffersMapped();

	// Global descriptor set accessors (one per frame, set=0 slot)
	static VkDescriptorSetLayout getGlobalDescriptorSetLayout();
	static std::vector<VkDescriptorSet>& getEditorGlobalDescriptorSets();
	static std::vector<VkDescriptorSet>& getGameGlobalDescriptorSets();

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
	static VkExtent2D swapChainExtent;
	static std::vector<VkImageView> swapChainImageViews;
	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	static VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	static uint32_t currentFrame;
	bool framebufferResized;
	bool swapChainRecreated;

	// Separate small GlobalUBO buffers for Editor and Game cameras
	static std::vector<VkBuffer> editorGlobalBuffers;
	std::vector<VkDeviceMemory> editorGlobalBuffersMemory;
	static std::vector<void*> editorGlobalBuffersMapped;

	static std::vector<VkBuffer> gameGlobalBuffers;
	std::vector<VkDeviceMemory> gameGlobalBuffersMemory;
	static std::vector<void*> gameGlobalBuffersMapped;

	// Global descriptor layout + pool + per-frame sets for Editor and Game
	static VkDescriptorSetLayout globalDescriptorSetLayout;
	VkDescriptorPool globalDescriptorPool;
	static std::vector<VkDescriptorSet> editorGlobalDescriptorSets;
	static std::vector<VkDescriptorSet> gameGlobalDescriptorSets;

	uint32_t mipLevels;
	VkSampler textureSampler;

	static VkSampleCountFlagBits msaaSamples;

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
														VkDebugUtilsMessageTypeFlagsEXT messageType,
														const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
														void* pUserData);

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

	VkSampleCountFlagBits getMaxUsableSampleCount();

	void loadModel();

	VkFormat
	findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	void createUniformBuffers();

	void createGlobalDescriptorSetLayout();
	void createGlobalDescriptorSets();

	void createTextureSampler();

	void createSyncObjects();

	void createCommandBuffers();

	void createCommandPool();

	void createRenderPass();

	VkShaderModule createShaderModule(const std::vector<char>& code);

	void createImageViews();

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

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

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	std::vector<const char*> getRequiredExtensions();

	void cleanupSwapChain();

public:
	void setEventBus(Core::EventBus* bus) {
		eventBus = bus;
	}

private:
	Core::EventBus* eventBus = nullptr;

	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
	static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
};
