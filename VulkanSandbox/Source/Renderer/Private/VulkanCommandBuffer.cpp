#include "VulkanCommandBuffer.h"
#include "VulkanMacros.h"

VulkanCommandBuffer::VulkanCommandBuffer(VulkanDevice* device, VulkanCommandBufferPool* command_buffer_pool, VkCommandBufferLevel level)
    : device_(device),
    command_buffer_pool_(command_buffer_pool)
{
    CHECK(device != nullptr);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    alloc_info.commandPool = command_buffer_pool_->GetHandle();
    alloc_info.level = level;   // VK_COMMAND_BUFFER_LEVEL_PRIMARY -> Can submit to queue for execution, can't call from other command buffers
                                // VK_COMMAND_BUFFER_LEVEL_SECONDARY -> Can't submit directly, can be called from primary command buffers.
    alloc_info.commandBufferCount = 1;

    VERIFY_VK_RESULT(vkAllocateCommandBuffers(device->GetLogicalDeviceHandle(), &alloc_info, &handle_));
}

VulkanCommandBuffer::~VulkanCommandBuffer()
{
    CHECK(handle_ != VK_NULL_HANDLE);
    CHECK(device_ != nullptr);
    CHECK(command_buffer_pool_ != nullptr);

    vkFreeCommandBuffers(device_->GetLogicalDeviceHandle(), command_buffer_pool_->GetHandle(), 1 /*num buffers*/, &handle_);
}
