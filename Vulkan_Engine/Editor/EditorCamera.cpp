#include "EditorCamera.h"

void EditorCamera::updateUniformBuffer(uint32_t currentImage) {
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(-55.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    ubo.view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

    ubo.proj = glm::perspective(glm::radians(45.0f), engineCore->getSwapChainExtent().width / (float) engineCore->getSwapChainExtent().height, 0.1f, 10.0f);

    ubo.proj[1][1] *= -1;

    memcpy(engineCore->getUniformBuffersMapped()[currentImage], &ubo, sizeof(ubo));
}