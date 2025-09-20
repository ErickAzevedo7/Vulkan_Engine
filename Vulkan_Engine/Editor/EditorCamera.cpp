#include "EditorCamera.h"  

//initialize static variables
glm::vec3 EditorCamera::cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);  
glm::vec3 EditorCamera::cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);  
glm::vec3 EditorCamera::cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);  
float EditorCamera::yaw = -90.0f;  
float EditorCamera::pitch = 0.0f;  
float EditorCamera::lastX = 0.0f; 
float EditorCamera::lastY = 0.0f;
bool EditorCamera::firstMouse = true;

void EditorCamera::init(VulkanCore* core) {  
   engineCore = core;
   int width, height;
   glfwGetFramebufferSize(engineCore->getWindow(), &width, &height);
   EditorCamera::lastX = width/2.0f;
   EditorCamera::lastY = height/2.0f;
}  

void EditorCamera::updateUniformBuffer(uint32_t currentImage) {  
   static auto startTime = std::chrono::high_resolution_clock::now();  

   auto currentTime = std::chrono::high_resolution_clock::now();  
   float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();  

   UniformBufferObject ubo{};  
   ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));  

   ubo.view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);  

   ubo.proj = glm::perspective(glm::radians(45.0f), engineCore->getSwapChainExtent().width / (float)engineCore->getSwapChainExtent().height, 0.1f, 10.0f);  

   ubo.proj[1][1] *= -1;  

   glfwSetCursorPosCallback(engineCore->getWindow(), mouse_callback);  

   memcpy(engineCore->getUniformBuffersMapped()[currentImage], &ubo, sizeof(ubo));  
}  

void EditorCamera::mouse_callback(GLFWwindow* window, double xpos, double ypos) {
   if (firstMouse)
   {
       lastX = xpos;
       lastY = ypos;
       firstMouse = false;
   }
   float xoffset = xpos - lastX;  
   float yoffset = lastY - ypos; // reversed since y-coordinates range from bottom to top  
   lastX = xpos;  
   lastY = ypos;  

   const float sensitivity = 0.1f;  
   xoffset *= sensitivity;  
   yoffset *= sensitivity;  

   yaw += xoffset;  
   pitch += yoffset;  
   if (pitch > 89.0f)  
       pitch = 89.0f;  
   if (pitch < -89.0f)  
       pitch = -89.0f;  

   glm::vec3 direction;  
   direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));  
   direction.y = sin(glm::radians(pitch));  
   direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));  
   cameraFront = glm::normalize(direction);  
}