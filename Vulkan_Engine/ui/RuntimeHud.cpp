#include "ui/RuntimeHud.h"

#include "context/ResourceContext.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "managers/TextureManager.h"
#include "renderer/vulkan/VulkanTexture.h"

ResourceContext* RuntimeHud::s_resources = nullptr;
std::vector<RuntimeHud::TextCommand> RuntimeHud::s_textCommands;
std::vector<RuntimeHud::ImageCommand> RuntimeHud::s_imageCommands;
std::unordered_map<std::string, VkDescriptorSet> RuntimeHud::s_textureSetCache;

namespace {
inline ImU32 toImColor(const glm::vec4& color) {
	const ImVec4 c(color.x, color.y, color.z, color.w);
	return ImGui::ColorConvertFloat4ToU32(c);
}
} // namespace

void RuntimeHud::initialize(ResourceContext* resources) {
	s_resources = resources;
	s_textCommands.clear();
	s_imageCommands.clear();
	s_textureSetCache.clear();
}

void RuntimeHud::shutdown() {
	s_textCommands.clear();
	s_imageCommands.clear();
	s_textureSetCache.clear();
	s_resources = nullptr;
}

void RuntimeHud::beginFrame() {
	s_textCommands.clear();
	s_imageCommands.clear();
}

void RuntimeHud::addText(const std::string& text,
						 const glm::vec2& position,
						 const glm::vec4& color,
						 float scale,
						 bool centered) {
	TextCommand cmd;
	cmd.text = text;
	cmd.position = position;
	cmd.color = color;
	cmd.scale = scale;
	cmd.centered = centered;
	s_textCommands.push_back(cmd);
}

void RuntimeHud::addImage(const std::string& texturePath,
						  const glm::vec2& position,
						  const glm::vec2& size,
						  const glm::vec4& tint,
						  bool centered) {
	ImageCommand cmd;
	cmd.texturePath = texturePath;
	cmd.position = position;
	cmd.size = size;
	cmd.tint = tint;
	cmd.centered = centered;
	s_imageCommands.push_back(cmd);
}

VkDescriptorSet RuntimeHud::resolveTextureSet(const std::string& texturePath) {
	if (!s_resources) {
		return VK_NULL_HANDLE;
	}

	auto it = s_textureSetCache.find(texturePath);
	if (it != s_textureSetCache.end()) {
		return it->second;
	}

	Texture* texture = nullptr;
	try {
		texture = s_resources->getTextureManager().getTexture(texturePath);
	} catch (...) {
		return VK_NULL_HANDLE;
	}

	if (!texture || !texture->handle.isValid() || !texture->sampler.isValid()) {
		return VK_NULL_HANDLE;
	}

	auto* graphicsTexture = s_resources->getTextureManager().getGraphicsTexture();
	auto* vulkanTexture = static_cast<Renderer::VulkanTexture*>(graphicsTexture);
	if (!vulkanTexture) {
		return VK_NULL_HANDLE;
	}

	VkSampler sampler = vulkanTexture->getSampler(texture->sampler);
	VkImageView view = vulkanTexture->getImageView(texture->handle);
	if (sampler == VK_NULL_HANDLE || view == VK_NULL_HANDLE) {
		return VK_NULL_HANDLE;
	}

	VkDescriptorSet set = ImGui_ImplVulkan_AddTexture(sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	if (set != VK_NULL_HANDLE) {
		s_textureSetCache.emplace(texturePath, set);
	}
	return set;
}

void RuntimeHud::draw(ImDrawList* drawList, const ImVec2& imageMin, const ImVec2& imageMax) {
	if (!drawList) {
		return;
	}

	const float width = imageMax.x - imageMin.x;
	const float height = imageMax.y - imageMin.y;

	for (const TextCommand& cmd : s_textCommands) {
		ImVec2 pos(imageMin.x + cmd.position.x * width, imageMin.y + cmd.position.y * height);
		if (cmd.centered) {
			ImVec2 textSize = ImGui::CalcTextSize(cmd.text.c_str());
			textSize.x *= cmd.scale;
			textSize.y *= cmd.scale;
			pos.x -= textSize.x * 0.5f;
			pos.y -= textSize.y * 0.5f;
		}

		if (cmd.scale != 1.0f) {
			ImGui::SetWindowFontScale(cmd.scale);
		}
		drawList->AddText(pos, toImColor(cmd.color), cmd.text.c_str());
		if (cmd.scale != 1.0f) {
			ImGui::SetWindowFontScale(1.0f);
		}
	}

	for (const ImageCommand& cmd : s_imageCommands) {
		VkDescriptorSet set = resolveTextureSet(cmd.texturePath);
		if (set == VK_NULL_HANDLE) {
			continue;
		}

		ImVec2 size(cmd.size.x * width, cmd.size.y * height);
		ImVec2 pos(imageMin.x + cmd.position.x * width, imageMin.y + cmd.position.y * height);
		if (cmd.centered) {
			pos.x -= size.x * 0.5f;
			pos.y -= size.y * 0.5f;
		}

		ImVec2 minPos(pos.x, pos.y);
		ImVec2 maxPos(pos.x + size.x, pos.y + size.y);
		drawList->AddImage(reinterpret_cast<ImTextureID>(set), minPos, maxPos, ImVec2(0, 0), ImVec2(1, 1),
						   toImColor(cmd.tint));
	}
}
