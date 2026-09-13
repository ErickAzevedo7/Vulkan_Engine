#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>
#include <string>

namespace VKE::RHI {

    class RHIDevice;

    // A single compiled shader stage, loaded from a file on disk.
    // Short-lived by design: RHIPipeline creates the shaders it needs, uses
    // them to build the pipeline, then releases them — only the compiled
    // pipeline is retained.
    class RHIShader {
    public:
        virtual ~RHIShader() = default;

        static Iunique<RHIShader> CreateFromFile(RHIDevice& device, const std::string& spirvPath, ShaderStageKind stage);

        virtual ShaderStageKind GetStage() const = 0;
    };

} // namespace VKE::RHI
