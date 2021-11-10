#pragma once
#include "vulkan/vulkan_core.h"

class VulkanRHI;

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;  // min/max number of images in swap chain, min/max width and height of images
    std::vector<VkSurfaceFormatKHR> surface_formats;    // pixel format, color space
    std::vector<VkPresentModeKHR> present_modes;    // conditions for "swapping" images to the screen
};

class VulkanSwapChain
{
public:
    VulkanSwapChain(VulkanRHI* RHI, VkSurfaceKHR surface, uint32 width, uint32 height);

    void Destroy();
    void Recreate();

    const std::vector<VkImage>& GetSwapChainImages()
    {
        CHECK(swapchain_handle_ != VK_NULL_HANDLE);
        return swap_chain_images_;
    }

    VkSurfaceFormatKHR GetSurfaceFormat() const
    {
        CHECK(swapchain_handle_ != VK_NULL_HANDLE);
        return surface_format_;
    }

    VkPresentModeKHR GetPresentMode() const
    {
        CHECK(swapchain_handle_ != VK_NULL_HANDLE);
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

private:
    SwapChainSupportDetails QuerySwapChainSupport();
    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats);
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& available_present_modes);
    VkExtent2D ChooseImageExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32 desired_width, uint32 desired_height);
    uint32 ChooseNumberOfImages(const VkSurfaceCapabilitiesKHR& capabilities);

    VulkanRHI* RHI_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    
    VkSurfaceFormatKHR surface_format_;
    VkPresentModeKHR present_mode_;
    VkExtent2D image_extent_;

    VkSwapchainKHR swapchain_handle_ = VK_NULL_HANDLE;
    std::vector<VkImage> swap_chain_images_;
};
