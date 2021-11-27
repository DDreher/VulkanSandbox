#include "VulkanCommandBufferPool.h"
#include "VulkanMacros.h"

VulkanCommandBufferPool::VulkanCommandBufferPool(VulkanDevice* device, VkCommandPoolCreateFlags flags)
    : device_(device)
{
    CHECK(device != nullptr);

    VkCommandPoolCreateInfo cmd_pool_create_info{};
    cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    // For now we only use drawing commands, so we stick to the graphics queue family.
    // TODO: Expose a way to create pools for other queue types.
    cmd_pool_create_info.queueFamilyIndex = device->GetGraphicsQueue()->GetFamilyIndex();   
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
