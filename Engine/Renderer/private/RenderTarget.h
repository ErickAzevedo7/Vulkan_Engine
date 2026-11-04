#pragma once

#include <core/Types.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIFramebuffer.h>
#include <rhi/RHIRenderPass.h>
#include <rhi/RHITexture.h>

namespace VKE {

// Offscreen color and depth pair that a pass renders into, built from the
// textures and framebuffer the RHI already provides.
struct RenderTarget {
	Iunique<RHITexture> Color;
	Iunique<RHITexture> Depth;
	Iunique<RHIFramebuffer> Framebuffer;
	u32 Width = 0;
	u32 Height = 0;

	void Create(RHIDevice &device, RHIRenderPass &renderPass, u32 width, u32 height, Format colorFormat,
			Format depthFormat, const char *debugName) {
		Width = width;
		Height = height;

		TextureDesc colorDesc;
		colorDesc.Width = width;
		colorDesc.Height = height;
		colorDesc.Format = colorFormat;
		colorDesc.Usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
		colorDesc.DebugName = debugName;
		Color = RHITexture::Create(device, colorDesc);

		TextureDesc depthDesc;
		depthDesc.Width = width;
		depthDesc.Height = height;
		depthDesc.Format = depthFormat;
		depthDesc.Usage = TextureUsage::DepthAttachment;
		depthDesc.DebugName = debugName;
		Depth = RHITexture::Create(device, depthDesc);

		FramebufferDesc framebufferDesc;
		framebufferDesc.AttachmentViews = { Color->GetAttachmentView(0, 0), Depth->GetAttachmentView(0, 0) };
		framebufferDesc.Width = width;
		framebufferDesc.Height = height;
		Framebuffer = RHIFramebuffer::Create(device, renderPass, framebufferDesc);
	}
};

} // namespace VKE
