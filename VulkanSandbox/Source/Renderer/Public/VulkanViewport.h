#pragma once
#include "vulkan/vulkan_core.h"

#include "VulkanRHI.h"
#include "VulkanSwapChain.h"

class VulkanViewport
{
public:
    VulkanViewport(VulkanRHI* RHI, VkSurfaceKHR surface, uint32 width, uint32 height);
    ~VulkanViewport();

    inline VulkanSwapChain* GetSwapChain() const
    {
        return swapchain_;
    }

    inline uint32 GetWidth() const
    {
        return width_;
    }

    inline uint32 GetHeight() const
    {
        return height_;
    }

    const std::vector<VkImageView>& GetBackBufferImageViews() const
    {
        return backbuffer_image_views_;
    }
    
    // TODO: I guess these shouldn't be public?
    void CreateSwapchain();
    void RecreateSwapchain();
    void DestroySwapchain();

private:
    
    void Resize(uint32 width, uint32 height);
   
    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t num_mips);

    VulkanRHI* RHI_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    VulkanSwapChain* swapchain_ = nullptr;
    uint32 width_ = 0;
    uint32 height_ = 0;

    std::vector<VkImageView> backbuffer_image_views_;   // Will be explicitly created by us -> We have to clean them up!
};
