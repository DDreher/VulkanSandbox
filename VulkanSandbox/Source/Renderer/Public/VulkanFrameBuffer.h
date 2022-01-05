#pragma once
#include "VulkanContext.h"

class VulkanFrameBuffer
{
public:
    VulkanFrameBuffer(VulkanContext* RHI, uint32 width, uint32 height, VkRenderPass render_pass, const std::vector<VkImageView>& attachments);
    ~VulkanFrameBuffer();

    void Destroy();

    inline VkFramebuffer GetHandle()
    {
        return handle_;
    }

private: 
    VulkanContext* RHI_ = nullptr;

    VkFramebuffer handle_ = VK_NULL_HANDLE;
};
