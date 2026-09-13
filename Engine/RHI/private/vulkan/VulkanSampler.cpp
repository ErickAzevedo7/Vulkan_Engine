#include <core/Log.h>
#include <rhi/RHISampler.h>

// Not implemented yet — see RHITexture.h's note; a sampler has no purpose
// without a texture to sample.

namespace VKE::RHI {

    Iunique<RHISampler> RHISampler::Create(RHIDevice& /*device*/, const SamplerDesc& /*desc*/) {
        VKE_CORE_WARN("RHISampler::Create: not implemented yet");
        return nullptr;
    }

} // namespace VKE::RHI
