#include "VulkanQueue.h"

#include "VulkanDevice.h"

VulkanQueue::VulkanQueue(VulkanDevice* device, uint32 family_idx)
    : device_(device),
    family_idx_(family_idx)
{
    CHECK(device != nullptr);

    vkGetDeviceQueue(device_->GetLogicalDeviceHandle(), family_idx_, queue_idx_, &queue_handle_);
    CHECK(queue_handle_ != VK_NULL_HANDLE);
}

VulkanQueue::~VulkanQueue()
{
}

void VulkanQueue::Submit(const VulkanCommandBuffer& command_buffer) const
{
    CHECK(command_buffer.IsInProgress() == false);

    VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer.GetHandle();
    vkQueueSubmit(device_->GetGraphicsQueue()->GetHandle(), 1, &submit_info, VK_NULL_HANDLE);

    // TODO: Add support for fences and semaphores
    // -> Would allow us to schedule multiple transfers at the same time instead of doing one transfer at a time.

    // For now execute transfer immediately.
    vkQueueWaitIdle(queue_handle_);
}
