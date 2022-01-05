#pragma once
#include <vulkan/vulkan_core.h>

#include "VulkanDevice.h"
#include "VulkanRenderPass.h"

struct VulkanFrameBuffer
{
    static VkFramebuffer Create(VulkanDevice* device, uint32 width, uint32 height,
        const std::vector<VkImageView>& views, const VulkanRenderPass& render_pass);
};
