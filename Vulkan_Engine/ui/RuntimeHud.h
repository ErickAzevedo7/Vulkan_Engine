#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "engine_api/EngineExport.h"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float4.hpp"
#include "vulkan/vulkan_core.h"

struct ImDrawList;
struct ImVec2;
class ResourceContext;

class ENGINE_API RuntimeHud {
public:
	// TEMPORARY HUD API
	static void initialize(ResourceContext* resources);
	static void shutdown();
	static void beginFrame();

	// TEMPORARY HUD API
	static void addText(const std::string& text,
						const glm::vec2& position,
						const glm::vec4& color,
						float scale = 1.0f,
						bool centered = false);

	// TEMPORARY HUD API
	static void addImage(const std::string& texturePath,
						 const glm::vec2& position,
						 const glm::vec2& size,
						 const glm::vec4& tint,
						 bool centered = false);

	// TEMPORARY HUD API
	static void draw(ImDrawList* drawList, const ImVec2& imageMin, const ImVec2& imageMax);

private:
	struct TextCommand {
		std::string text;
		glm::vec2 position{0.0f, 0.0f};
		glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
		float scale = 1.0f;
		bool centered = false;
	};

	struct ImageCommand {
		std::string texturePath;
		glm::vec2 position{0.0f, 0.0f};
		glm::vec2 size{0.0f, 0.0f};
		glm::vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
		bool centered = false;
	};

	static VkDescriptorSet resolveTextureSet(const std::string& texturePath);

	static ResourceContext* s_resources;
	static std::vector<TextCommand> s_textCommands;
	static std::vector<ImageCommand> s_imageCommands;
	static std::unordered_map<std::string, VkDescriptorSet> s_textureSetCache;
};
