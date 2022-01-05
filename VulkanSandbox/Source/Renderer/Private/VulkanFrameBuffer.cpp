#include "VulkanFrameBuffer.h"

#include "VulkanMacros.h"

VulkanFrameBuffer::VulkanFrameBuffer(VulkanContext* RHI, uint32 width, uint32 height, VkRenderPass render_pass, const std::vector<VkImageView>& attachments)
    : RHI_(RHI)
{
    // This has to be in the correct order, as specified in the render pass!
    // TODO: Currently hardcoded. Should be specified from outside.
    //std::array<VkImageView, 3> attachments =
    //{
    //    color_image_view_,
    //    depth_image_view_,  // depth buffer can be used by all of the swap chain images, because only a single subpass is running at the same time
    //    swap_chain_image_views_[i] // Color attachment differs for every swap chain image
    //};

    VkFramebufferCreateInfo frambuffer_info{};
    frambuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    //frambuffer_info.renderPass = render_pass_;  // Framebuffer needs to be compatible with this render pass -> Use same number and types of attachments
    frambuffer_info.attachmentCount = static_cast<uint32>(attachments.size());
    frambuffer_info.pAttachments = attachments.data(); // Specify the VkImageView objects that should be bound to the respective attachment descriptions
                                                       // in the render pass pAttachment array.
    frambuffer_info.width = width;
    frambuffer_info.height = height;
    frambuffer_info.layers = 1; // swap chain images are single images -> 1 layer.

    VERIFY_VK_RESULT(vkCreateFramebuffer(RHI_->GetDevice()->GetLogicalDeviceHandle(), &frambuffer_info, nullptr, &handle_));
}
