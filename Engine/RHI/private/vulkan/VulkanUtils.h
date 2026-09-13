#pragma once

// Translation helpers + error-checking macro shared by every Vulkan backend
// file. Like Platform/private/glfw/GLFWKeyMap.h, this is one of the few
// places in the engine allowed to know Vulkan's vocabulary directly.

#include <core/Assert.h>
#include <core/Log.h>
#include <rhi/RHITypes.h>
#include <vulkan/vulkan.h>

namespace VKE::RHI {

    // Aborts (debug break) on any non-VK_SUCCESS result, after logging it.
    // Every Vulkan backend file uses this instead of ignoring VkResult —
    // a create/allocate failure here means nothing downstream is safe to run.
#define VK_CHECK(expr)                                                                                               \
    do {                                                                                                             \
        VkResult vkCheckResult_ = (expr);                                                                            \
        if (vkCheckResult_ != VK_SUCCESS) {                                                                          \
            VKE_CORE_ERROR("Vulkan error {} at {}:{} — {}", static_cast<int>(vkCheckResult_), __FILE__, __LINE__,    \
                            #expr);                                                                                  \
            VKE_DEBUGBREAK();                                                                                        \
        }                                                                                                            \
    } while (0)

    inline VkFormat ToVkFormat(Format format) {
        switch (format) {
            case Format::R32_SFLOAT: return VK_FORMAT_R32_SFLOAT;
            case Format::R32G32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
            case Format::R32G32B32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
            case Format::R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
            case Format::R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
            case Format::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
            case Format::B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
            case Format::B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
            case Format::D32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
            case Format::D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
            case Format::D32_SFLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
            default: return VK_FORMAT_UNDEFINED;
        }
    }

    inline Format FromVkFormat(VkFormat format) {
        switch (format) {
            case VK_FORMAT_R32_SFLOAT: return Format::R32_SFLOAT;
            case VK_FORMAT_R32G32_SFLOAT: return Format::R32G32_SFLOAT;
            case VK_FORMAT_R32G32B32_SFLOAT: return Format::R32G32B32_SFLOAT;
            case VK_FORMAT_R32G32B32A32_SFLOAT: return Format::R32G32B32A32_SFLOAT;
            case VK_FORMAT_R8G8B8A8_UNORM: return Format::R8G8B8A8_UNORM;
            case VK_FORMAT_R8G8B8A8_SRGB: return Format::R8G8B8A8_SRGB;
            case VK_FORMAT_B8G8R8A8_UNORM: return Format::B8G8R8A8_UNORM;
            case VK_FORMAT_B8G8R8A8_SRGB: return Format::B8G8R8A8_SRGB;
            case VK_FORMAT_D32_SFLOAT: return Format::D32_SFLOAT;
            case VK_FORMAT_D24_UNORM_S8_UINT: return Format::D24_UNORM_S8_UINT;
            case VK_FORMAT_D32_SFLOAT_S8_UINT: return Format::D32_SFLOAT_S8_UINT;
            default: return Format::Undefined;
        }
    }

    inline VkPrimitiveTopology ToVkTopology(PrimitiveTopology topology) {
        switch (topology) {
            case PrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case PrimitiveTopology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case PrimitiveTopology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            default: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }
    }

    inline VkPolygonMode ToVkPolygonMode(PolygonMode mode) {
        switch (mode) {
            case PolygonMode::Fill: return VK_POLYGON_MODE_FILL;
            case PolygonMode::Line: return VK_POLYGON_MODE_LINE;
            case PolygonMode::Point: return VK_POLYGON_MODE_POINT;
            default: return VK_POLYGON_MODE_FILL;
        }
    }

    inline VkCullModeFlags ToVkCullMode(CullMode mode) {
        switch (mode) {
            case CullMode::None: return VK_CULL_MODE_NONE;
            case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
            case CullMode::Back: return VK_CULL_MODE_BACK_BIT;
            default: return VK_CULL_MODE_BACK_BIT;
        }
    }

    inline VkFrontFace ToVkFrontFace(FrontFace face) {
        return face == FrontFace::Clockwise ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }

    inline VkCompareOp ToVkCompareOp(CompareOp op) {
        switch (op) {
            case CompareOp::Never: return VK_COMPARE_OP_NEVER;
            case CompareOp::Less: return VK_COMPARE_OP_LESS;
            case CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
            case CompareOp::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
            case CompareOp::Greater: return VK_COMPARE_OP_GREATER;
            case CompareOp::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
            case CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
            default: return VK_COMPARE_OP_LESS;
        }
    }

} // namespace VKE::RHI
