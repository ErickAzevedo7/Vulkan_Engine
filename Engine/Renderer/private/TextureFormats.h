#pragma once

#include <rhi/RHITypes.h>

namespace VKE::Renderer {

    // Formats whose samples come back as linear color rather than the stored values.
    inline bool SamplesAsLinear(RHI::Format format) {
        switch (format) {
            case RHI::Format::R8G8B8A8_SRGB:
            case RHI::Format::B8G8R8A8_SRGB:
            case RHI::Format::R16G16B16A16_SFLOAT:
            case RHI::Format::R32G32B32A32_SFLOAT: return true;
            default: return false;
        }
    }

} // namespace VKE::Renderer
