#pragma once
#include "vulkan/vulkan_core.h"
 
#include "VulkanRHI.h"
#include "VulkanSwapChain.h"

struct DepthStencilAttachmentInfo
{

};

struct ColorAttachmentInfo
{

};

struct RenderPassInfo
{
    uint32 num_samples = 1;
};

/**
 * Wrapper around vkRenderPass.
 * A RenderPass specifies how many color and depth buffers there will be, how many samples to use for each of them and
 * how their contents should be handled throughout the rendering operations
 */
class VulkanRenderPass
{
public:
    VulkanRenderPass(VulkanRHI* RHI, VulkanSwapChain* swapchain);
    ~VulkanRenderPass();

    inline VkRenderPass GetHandle() const
    {
        return render_pass_;
    }

private:
    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VulkanRHI* RHI_ = nullptr;
};
