#include "VulkanQueue.h"

#include "VulkanDevice.h"

VulkanQueue::VulkanQueue(VulkanDevice* device, uint32 family_idx)
    : device_(device),
    family_idx_(family_idx)
{
    vkGetDeviceQueue(device_->GetLogicalDeviceHandle(), family_idx_, queue_idx_, &queue_handle_);
    CHECK(queue_handle_ != VK_NULL_HANDLE);
}

VulkanQueue::~VulkanQueue()
{
}
