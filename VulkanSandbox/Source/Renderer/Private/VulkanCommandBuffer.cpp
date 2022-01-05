#include "VulkanCommandBuffer.h"

#include "VulkanDevice.h"
#include "VulkanMacros.h"
#include "VulkanQueue.h"

VulkanCommandBufferPool::VulkanCommandBufferPool(VulkanDevice* device, VulkanQueue* queue, const VkCommandPoolCreateFlags& flags)
    : device_(device)
{
    CHECK(device != nullptr);
    CHECK(queue != nullptr);
    VkCommandPoolCreateInfo cmd_pool_create_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cmd_pool_create_info.queueFamilyIndex = queue->GetFamilyIndex();
    cmd_pool_create_info.flags = flags; // Optional.
                                        // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Command buffers are rerecorded with new commands very often
                                        // (may change memory allocation behavior)
                                        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be rerecorded individually.
                                        // Without this flag they all have to be reset together.
    VERIFY_VK_RESULT(vkCreateCommandPool(device->GetLogicalDeviceHandle(), &cmd_pool_create_info, nullptr, &handle_));
}

VulkanCommandBufferPool::~VulkanCommandBufferPool()
{
    CHECK(handle_ != VK_NULL_HANDLE);
    CHECK(device_ != nullptr);
    vkDestroyCommandPool(device_->GetLogicalDeviceHandle(), handle_, nullptr);
    // ^^^ Also destroys any command buffers we retrieved from the pool
}


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

void VulkanCommandBuffer::Begin(const VkCommandBufferUsageFlags& flags)
{
    VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin_info.flags = flags;
    VERIFY_VK_RESULT(vkBeginCommandBuffer(handle_, &begin_info));
    is_record_in_progress = true;
}

void VulkanCommandBuffer::End()
{
    vkEndCommandBuffer(GetHandle());
    is_record_in_progress = false;
}
