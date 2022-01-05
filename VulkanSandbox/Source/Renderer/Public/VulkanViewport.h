#pragma once
#include "vulkan/vulkan_core.h"

#include "VulkanRHI.h"
#include "VulkanSwapChain.h"

/**
 * Thin abstraction to encapsulate all things needed to present something with Vulkan.
 * Owns the swapchain and the corresponding image views
 */
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
   
    VulkanRHI* RHI_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    VulkanSwapChain* swapchain_ = nullptr;
    uint32 width_ = 0;
    uint32 height_ = 0;

    std::vector<VkImageView> backbuffer_image_views_;   // Will be explicitly created by us -> We have to clean them up!
};
