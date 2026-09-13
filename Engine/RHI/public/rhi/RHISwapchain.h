#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>

namespace VKE::Platform {
    class IWindow;
} // namespace VKE::Platform

namespace VKE::RHI {

    class RHIDevice;
    class RHICommandBuffer;

    // Owns the presentable images the engine renders into, and the
    // synchronization needed to reuse them safely across frames.
    class RHISwapchain {
    public:
        virtual ~RHISwapchain() = default;

        static Iunique<RHISwapchain> Create(RHIDevice& device, Platform::IWindow& window);

        // Rebuilds the swapchain at the window's current size. Call after a
        // resize, or when AcquireNextImage/SubmitAndPresent report it out
        // of date.
        virtual void Recreate() = 0;

        virtual u32 GetWidth() const = 0;
        virtual u32 GetHeight() const = 0;
        virtual Format GetColorFormat() const = 0;
        virtual u32 GetImageCount() const = 0;

        // How many frames the swapchain lets the CPU get ahead of the GPU.
        // A caller keeping its own per-frame resources (e.g. command
        // buffers) needs one per slot, selected by GetCurrentFrameIndex().
        virtual u32 GetFramesInFlightCount() const = 0;

        // Which frame-in-flight slot the frame being recorded right now
        // uses. Stable between AcquireNextImage() and SubmitAndPresent() —
        // read it after Acquire to know which of your own per-frame
        // resources are now safe to reuse.
        virtual u32 GetCurrentFrameIndex() const = 0;

        // Opaque view for swapchain image `index`, used as an
        // RHIFramebuffer color attachment.
        virtual void* GetImageView(u32 index) const = 0;

        // Acquires the next image to render into. Returns false if the
        // swapchain is out of date (caller should Recreate() and skip
        // drawing this frame).
        virtual bool AcquireNextImage(u32& outImageIndex) = 0;

        // Submits `cmd` and presents the image acquired by the last
        // AcquireNextImage() call. Returns false if the swapchain is out of
        // date/suboptimal (caller should Recreate()).
        virtual bool SubmitAndPresent(RHICommandBuffer& cmd) = 0;
    };

} // namespace VKE::RHI
