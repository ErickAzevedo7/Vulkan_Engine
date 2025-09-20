#pragma once  
#define GLM_ENABLE_EXPERIMENTAL  
#include <vulkan/vulkan.h>  
#include "vulkancore.h"  

class EditorCamera  
{  
public:  
	static glm::vec3 cameraPos;
	static glm::vec3 cameraFront;
	static glm::vec3 cameraUp;
	static glm::vec3 direction;  
	static float yaw;  
	static float pitch;  
	static float lastX;
	static float lastY;
	static bool firstMouse;
	void init(VulkanCore* core);  

	void updateUniformBuffer(uint32_t currentImage);  
	static void mouse_callback(GLFWwindow* window, double xpos, double ypos);  
private:  
	VulkanCore* engineCore;  
};  
