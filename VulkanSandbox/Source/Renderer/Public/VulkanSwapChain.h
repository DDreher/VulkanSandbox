#pragma once
#include <vulkan/vulkan_core.h>

#include "VulkanContext.h"

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;  // min/max number of images in swap chain, min/max width and height of images
    std::vector<VkSurfaceFormatKHR> surface_formats;    // pixel format, color space
    std::vector<VkPresentModeKHR> present_modes;    // conditions for "swapping" images to the screen
};

class VulkanSwapChain
{
public:
    VulkanSwapChain(VulkanDevice* device, VkSurfaceKHR surface, uint32 width, uint32 height);

    void Destroy();
    void Recreate();

    const std::vector<VkImage>& GetSwapChainImages()
    {
        CHECK(swapchain_ != VK_NULL_HANDLE);
        return swap_chain_images_;
    }

    VkSurfaceFormatKHR GetSurfaceFormat() const
    {
        CHECK(swapchain_ != VK_NULL_HANDLE);
        return surface_format_;
    }

    VkPresentModeKHR GetPresentMode() const
    {
        CHECK(swapchain_ != VK_NULL_HANDLE);
        return present_mode_;
    }

    const VkExtent2D& GetImageExtent() const
    {
        return image_extent_;
    }

    const std::vector<VkImage>& GetSwapChainImages() const
    {
        return swap_chain_images_;
    }

    const std::vector<VkImageView>& GetSwapChainImageViews() const
    {
        return swap_chain_image_views_;
    }

    inline VkSwapchainKHR GetHandle() const
    {
        return swapchain_;
    }

private:
    SwapChainSupportDetails QuerySwapChainSupport();
    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats);
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& available_present_modes);
    VkExtent2D ChooseImageExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32 desired_width, uint32 desired_height);
    uint32 ChooseNumberOfImages(const VkSurfaceCapabilitiesKHR& capabilities);

    VulkanDevice* device_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    
    VkSurfaceFormatKHR surface_format_;
    VkPresentModeKHR present_mode_;
    VkExtent2D image_extent_;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;

    // We don't use the VulkanImage wrapper here, because the vkImage object are actually managed internally by the vkSwapchain!
    std::vector<VkImage> swap_chain_images_;
    std::vector<VkImageView> swap_chain_image_views_;
};
