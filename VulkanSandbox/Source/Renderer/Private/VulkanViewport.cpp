#include "VulkanViewport.h"

#include "VulkanSwapchain.h"
#include "VulkanContext.h"
#include "VulkanMacros.h"

VulkanViewport::VulkanViewport(VulkanDevice* device, VkSurfaceKHR surface, uint32 width, uint32 height)
    : device_(device),
    surface_(surface),
    width_(width),
    height_(height)
{
    CHECK(device != nullptr);
    CHECK(surface != VK_NULL_HANDLE);
    swapchain_ = new VulkanSwapchain(device, surface_, width_, height_);
}

VulkanViewport::~VulkanViewport()
{
    DestroySwapchain();
}

void VulkanViewport::DestroySwapchain()
{
    if (swapchain_ != nullptr)
    {
        CHECK(swapchain_ != nullptr);
        swapchain_->Destroy();
        delete swapchain_;
        swapchain_ = nullptr;
    }
}

void VulkanViewport::Resize(uint32 width, uint32 height)
{

}
