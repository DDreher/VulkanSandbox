#include "VulkanFrameBuffer.h"

#include "VulkanMacros.h"

VkFramebuffer VulkanFrameBuffer::Create(VulkanDevice* device, uint32 width, uint32 height, const std::vector<VkImageView>& views, const VulkanRenderPass& render_pass)
{
    CHECK(device != nullptr);

    VkFramebufferCreateInfo frambuffer_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    frambuffer_info.renderPass = render_pass.GetHandle();  // Framebuffer needs to be compatible with this render pass => use same number of attachments
    frambuffer_info.attachmentCount = static_cast<uint32>(views.size());
    frambuffer_info.pAttachments = views.data();
    frambuffer_info.width = width;
    frambuffer_info.height = height;
    frambuffer_info.layers = 1; // swap chain images are single images -> 1 layer.

    VkFramebuffer frame_buffer;
    VERIFY_VK_RESULT(vkCreateFramebuffer(device->GetLogicalDeviceHandle(), &frambuffer_info, nullptr, &frame_buffer));
    return frame_buffer;
}
