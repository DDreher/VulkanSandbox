#pragma once
#include "vulkan/vulkan_core.h"

#include "VulkanContext.h"
#include "VulkanSwapchain.h"

/**
 * Thin abstraction to encapsulate all things needed to present something with Vulkan.
 * Owns the swapchain and the corresponding image views
 */
class VulkanViewport
{
public:
    VulkanViewport(VulkanDevice* device, VkSurfaceKHR surface, uint32 width, uint32 height);
    ~VulkanViewport();

    VulkanSwapchain* GetSwapChain() const
    {
        return swapchain_;
    }

    uint32 GetWidth() const
    {
        return width_;
    }

    uint32 GetHeight() const
    {
        return height_;
    }

    void DestroySwapchain();

    void Resize(uint32 width, uint32 height);
private:
    
   
    VulkanDevice* device_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    VulkanSwapchain* swapchain_ = nullptr;
    uint32 width_ = 0;
    uint32 height_ = 0;
};
