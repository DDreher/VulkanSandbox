#include "VulkanViewport.h"

#include "VulkanSwapChain.h"
#include "VulkanContext.h"
#include "VulkanMacros.h"

VulkanViewport::VulkanViewport(VulkanContext* RHI, VkSurfaceKHR surface, uint32 width, uint32 height)
    : RHI_(RHI),
    surface_(surface),
    width_(width),
    height_(height)
{
    CreateSwapchain();
}

VulkanViewport::~VulkanViewport()
{
    // TODO: Temporary null check. Should check if the viewport may take care of the entire swapchain creation/deletion stuff
    if(swapchain_ != nullptr)
    {
        CHECK(swapchain_ != nullptr);
        swapchain_->Destroy();
        delete swapchain_;
        swapchain_ = nullptr;
    }
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
        backbuffer_image_views_[i] = RHI_->CreateImageView(backbuffer_images[i], swapchain_->GetSurfaceFormat().format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
}

void VulkanViewport::RecreateSwapchain()
{

}

void VulkanViewport::DestroySwapchain()
{
    for (auto image_view : backbuffer_image_views_)
    {
        vkDestroyImageView(RHI_->GetDevice()->GetLogicalDeviceHandle(), image_view, nullptr);
    }
    swapchain_->Destroy();
}

void VulkanViewport::Resize(uint32 width, uint32 height)
{

}
