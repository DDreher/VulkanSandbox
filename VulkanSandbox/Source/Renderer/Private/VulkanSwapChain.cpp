#include "VulkanSwapChain.h"

#include "VulkanMacros.h"
#include "VulkanQueue.h"

VulkanSwapChain::VulkanSwapChain(VulkanRHI* RHI, VkSurfaceKHR surface, uint32 width, uint32 height)
    : RHI_(RHI),
    surface_(surface)
{
    CHECK(RHI_ != nullptr);
    CHECK(RHI_->GetDevice() != nullptr);
    CHECK(surface_ != VK_NULL_HANDLE);

    SwapChainSupportDetails swapchain_support_details = QuerySwapChainSupport();
    surface_format_ = ChooseSurfaceFormat(swapchain_support_details.surface_formats);
    present_mode_ = ChoosePresentMode(swapchain_support_details.present_modes);
    image_extent_ = ChooseImageExtent(swapchain_support_details.capabilities, width, height);
    uint32 num_images = ChooseNumberOfImages(swapchain_support_details.capabilities);

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface_; // Swap chain is tied to this surface
    create_info.minImageCount = num_images;
    create_info.imageFormat = surface_format_.format;
    create_info.imageColorSpace = surface_format_.colorSpace;
    create_info.imageExtent = image_extent_;
    create_info.imageArrayLayers = 1;   // Amount of layers each image consists of. 1 unless developing a stereoscopic 3D application.
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;   // What kind of operations images in the swap chain are used for.
                                                                    // For now we'll render directly to them => Color attachment.
                                                                    // We could also first render to a separate image and then do some post-processing operations.
                                                                    // In that case we may use a value like VK_IMAGE_USAGE_TRANSFER_DST_BIT to use a memory 
                                                                    // operation to transfer the rendered image to a swap chain image.
    create_info.presentMode = present_mode_;
    create_info.clipped = VK_TRUE;  // If true, don't care about pixels that are obscured (e.g. by another window in front).
                                    // Clipping increases performance => only deactivate if really needed.

    // We can specify that a certain transform should be applied to images in the swap chain if it is supported
    // e.g. 90 degree clockwise rotation, horizontal flip,...
    create_info.preTransform = swapchain_support_details.capabilities.currentTransform;    // Simply set currentTransform to specify that we don't use any preTransform.

    // Specify if the alpha channel should be used for blending with other windows in the window system
    // VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR -> Ignore the alpha channel.
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    // Specify how to handle swap chain images that will be used across multiple queue families 
    // E.g. if graphics queue is different from presentation queue, we'll draw onto images in swap chain from the graphics queue 
    // and then submit them to the presentation queue
    uint32 queue_family_indices[] = {
                                      RHI_->GetDevice()->GetGraphicsQueue()->GetFamilyIndex(),
                                      RHI_->GetDevice()->GetPresentQueue()->GetFamilyIndex()
    };
    if (RHI_->GetDevice()->GetGraphicsQueue()->GetFamilyIndex() != RHI_->GetDevice()->GetPresentQueue()->GetFamilyIndex())
    {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;  // Images can be used across multiple queue families without explicit ownership transfers.
                                                                    // I check out ownership concepts at a later time, so for now we rely on concurrent mode.

        // Concurrent mode requires you to specify in advance between which queue families ownership will be shared
        create_info.queueFamilyIndexCount = 2;  // Has to be at least 2 to use concurrent mode!
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else
    {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;   // An image is owned by one queue family at a time and ownership must be explicitly transferred 
                                                                    // before using it in another queue family. This option offers the best performance.
        create_info.queueFamilyIndexCount = 0; // Optional
        create_info.pQueueFamilyIndices = nullptr; // Optional
    }

    // With Vulkan it's possible that the swap chain becomes invalid or unoptimized while the application is running (e.g. due to window resize).
    // => We may have to recreate swap chain from scratch. If so we have to provide a handle to the old swap chain here.
    create_info.oldSwapchain = VK_NULL_HANDLE;  // For now assume that we will only ever create one swap chain.

    LOG("Creating Vulkan swapchain (present mode: {}, format: {}, color space: {})",
        static_cast<uint32>(present_mode_), static_cast<uint32>(surface_format_.format), static_cast<uint32>(surface_format_.colorSpace));

    VkResult result = vkCreateSwapchainKHR(RHI_->GetDevice()->GetLogicalDeviceHandle(), &create_info, nullptr, &swapchain_);
    VERIFY_VK_RESULT(result);
    if (result != VK_SUCCESS)
    {
        LOG_ERROR("Failed to create swap chain!");
        exit(EXIT_FAILURE);
    }

    // Retrieve image handles of swap chain.
    // We only specified the minimum num of images => We have to check how much were created
    uint32 num_swapchain_images;
    VERIFY_VK_RESULT(vkGetSwapchainImagesKHR(RHI_->GetDevice()->GetLogicalDeviceHandle(), swapchain_, &num_swapchain_images, nullptr));
    swap_chain_images_.resize(num_swapchain_images);
    VERIFY_VK_RESULT(vkGetSwapchainImagesKHR(RHI_->GetDevice()->GetLogicalDeviceHandle(), swapchain_, &num_swapchain_images, swap_chain_images_.data()));
}

void VulkanSwapChain::Destroy()
{
    vkDestroySwapchainKHR(RHI_->GetDevice()->GetLogicalDeviceHandle(), swapchain_, nullptr);
}

void VulkanSwapChain::Recreate()
{

}

SwapChainSupportDetails VulkanSwapChain::QuerySwapChainSupport()
{
    SwapChainSupportDetails details;
    VkPhysicalDevice device = RHI_->GetDevice()->GetPhysicalDeviceHandle();
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

    uint32_t surface_format_count;
    VERIFY_VK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &surface_format_count, nullptr));
    details.surface_formats.resize(surface_format_count);
    VERIFY_VK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &surface_format_count, details.surface_formats.data()));

    uint32_t present_mode_count;
    VERIFY_VK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &present_mode_count, nullptr));
    if (present_mode_count > 0)
    {
        details.present_modes.resize(present_mode_count);
        VERIFY_VK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &present_mode_count, details.present_modes.data()));
    }
    else
    {
        LOG_ERROR("Failed to find present modes for surface!");
        exit(EXIT_FAILURE);
    }

    return details;
}

VkSurfaceFormatKHR VulkanSwapChain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats)
{
    for (const auto& surface_format : available_formats)
    {
        // For now we simply prefer SRGB if it is available 
        // SRGB results in more accurate perceived colors and is standard color space for images / textures.
        if (surface_format.format == VK_FORMAT_B8G8R8A8_SRGB && surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return surface_format;
        }
    }

    // If we can't find our preferred surface format, we could rank the available format and choose the next best...
    // For now we'll just use the first available format, which should be okay for most cases.
    LOG_WARN("VK_FORMAT_B8G8R8A8_SRGB not supported. Falling back to first supported surface format (format {}, color space {})",
        static_cast<uint32>(available_formats[0].format),
        static_cast<uint32>(available_formats[0].colorSpace));
    return available_formats[0];
}

VkPresentModeKHR VulkanSwapChain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& available_present_modes)
{
    CHECK(available_present_modes.size() > 0);

    // VK_PRESENT_MODE_IMMEDIATE_KHR - Submitted images are transferred to screen right away => Possible tearing. Guaranteed to be available.
    // VK_PRESENT_MODE_FIFO_KHR - Images are taken from fifo queue on display refresh. If queue is full the program has to wait -> Similar to vsync!
    // VK_PRESENT_MODE_FIFO_RELAXED_KHR - Similar to VK_PRESENT_MODE_FIFO_KHR, if swap chain is empty the next rendered image is shown instantly -> Possible tearing.
    // VK_PRESENT_MODE_MAILBOX_KHR - Similar to VK_PRESENT_MODE_FIFO_KHR, if queue is full the application just replaces the already queued images.
    // => Can be used for triple buffering.
    for (const auto& present_mode : available_present_modes)
    {
        // For now we simply prefer the mailbx present mode.
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return present_mode;
        }
    }

    LOG_WARN("VK_PRESENT_MODE_MAILBOX_KHR not supported. Falling back to first supported present mode ()",
        static_cast<uint32>(available_present_modes[0]));
    return available_present_modes[0];
}

VkExtent2D VulkanSwapChain::ChooseImageExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32 desired_width, uint32 desired_height)
{
    // Swap extent is the resolution of the swap chain images in PIXELS! We have to keep that in mind for high DPI screens, e.g. Retina displays.
    // Usually Vulkan tells us to match the window resolution and sets the extends by itself.
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }
    else // But...
    {
        // Some window managers allow extends that differ from window resolution, as indicated by setting the width and height in currentExtent to max of uint32_t.
        // In that case, pick the resolution that best matches the window within the minImageExtent and maxImageExtent bounds.

        // Important: Extent in PIXELS instead of screen coordinates.
        VkExtent2D actual_extent = {
            static_cast<uint32>(desired_width),
            static_cast<uint32>(desired_height)
        };

        // Clamp to range [minImageExtent, maxImageExtent]
        actual_extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actual_extent.width));
        actual_extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actual_extent.height));

        return actual_extent;
    }
}

uint32 VulkanSwapChain::ChooseNumberOfImages(const VkSurfaceCapabilitiesKHR& capabilities)
{
    // Check the max number of supported images
    // 0 means that there is no maximum set by the device!
    bool is_num_images_limited = capabilities.maxImageCount > 0;
    
    // Specify the minimum num images we would like to have in the swap chain
    // Minimum + 1 is recommended to avoid GPU stalls.
    uint32 desired_num_images = capabilities.minImageCount + 1;
    
    // Ensure we don't exceed the supported max image count in the swap chain. 
    if (is_num_images_limited && desired_num_images > capabilities.maxImageCount)
    {
        desired_num_images = capabilities.maxImageCount;
    }
    
    return desired_num_images;
}