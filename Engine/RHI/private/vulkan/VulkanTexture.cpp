#include <core/Log.h>
#include <rhi/RHITexture.h>

// Not implemented yet — see RHITexture.h's note. No concrete Vulkan
// implementation exists yet; nothing currently creates a texture.

namespace VKE::RHI {

    Iunique<RHITexture> RHITexture::Create(RHIDevice& /*device*/, const TextureDesc& desc, const void* /*initialData*/) {
        VKE_CORE_WARN("RHITexture::Create('{}'): not implemented yet", desc.DebugName ? desc.DebugName : "<unnamed>");
        return nullptr;
    }

} // namespace VKE::RHI
