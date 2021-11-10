#include "VulkanViewport.h"

#include "VulkanSwapChain.h"
#include "VulkanRHI.h"
#include "VulkanMacros.h"

VulkanViewport::VulkanViewport(VulkanRHI* RHI, VkSurfaceKHR surface, uint32 width, uint32 height)
    : RHI_(RHI),
    surface_(surface),
    width_(width),
    height_(height)
{
    CreateSwapchain();
}

VulkanViewport::~VulkanViewport()
{
    CHECK(swapchain_ != nullptr);
    swapchain_->Destroy();
    delete swapchain_;
    swapchain_ = nullptr;
}

void VulkanViewport::CreateSwapchain()
{
    CHECK(RHI_ != nullptr);
    CHECK(surface_ != VK_NULL_HANDLE);
    swapchain_ = new VulkanSwapChain(RHI_, surface_, width_, height_);

    auto& backbuffer_images = swapchain_->GetSwapChainImages();
    backbuffer_image_views_.resize(backbuffer_images.size());
    for (size_t i = 0; i < backbuffer_images.size(); ++i)
    {
        backbuffer_image_views_[i] = CreateImageView(backbuffer_images[i], swapchain_->GetSurfaceFormat().format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }

}

void VulkanViewport::RecreateSwapchain()
{

}

void VulkanViewport::DestroySwapchain()
{

}

void VulkanViewport::Resize(uint32 width, uint32 height)
{

}

VkImageView VulkanViewport::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t num_mips)
{
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = num_mips;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    VkResult result = vkCreateImageView(RHI_->GetDevice()->GetLogicalDeviceHandle(), &view_info, nullptr, &image_view);
    VERIFY_VK_RESULT(result);

    if (result != VK_SUCCESS)
    {
        LOG_ERROR("Failed to create image view!");
        exit(EXIT_FAILURE);
    }

    return image_view;
}
