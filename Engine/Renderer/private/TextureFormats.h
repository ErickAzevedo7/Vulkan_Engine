#pragma once

#include <rhi/RHITypes.h>

namespace VKE {

// Formats whose samples come back as linear color rather than the stored values.
inline bool SamplesAsLinear(Format format) {
	switch (format) {
		case Format::R8G8B8A8_SRGB:
		case Format::B8G8R8A8_SRGB:
		case Format::R16G16B16A16_SFLOAT:
		case Format::R32G32B32A32_SFLOAT:
			return true;
		default:
			return false;
	}
}

} // namespace VKE
