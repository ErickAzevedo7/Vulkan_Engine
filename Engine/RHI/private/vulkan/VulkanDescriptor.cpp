#include <core/Log.h>
#include <rhi/RHIDescriptor.h>

// Not implemented yet — see RHITexture.h's note. No pipeline currently
// uses a descriptor set, so nothing calls this yet.

namespace VKE::RHI {

    Iunique<RHIDescriptorSet> RHIDescriptorSet::Create(RHIDevice& /*device*/, const DescriptorSetLayoutDesc& /*desc*/) {
        VKE_CORE_WARN("RHIDescriptorSet::Create: not implemented yet");
        return nullptr;
    }

} // namespace VKE::RHI
