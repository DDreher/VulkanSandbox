#pragma once
#include "vulkan/vulkan_core.h"

#include "VulkanDevice.h"

/*
 * Wrapper around VkCommandPool.
 * Used to allocate command buffers for a given queue type.
 * 
 * Drawing operations and memory transfers are stored in command buffers. These are retrieved from command pools.
 * We can fill these buffers in multiple threads and then execute them all at once on the render thread.
 */
class VulkanCommandBufferPool
{
public:
    VulkanCommandBufferPool(VulkanDevice* device, VkCommandPoolCreateFlags flags = 0);
    ~VulkanCommandBufferPool();

    inline VkCommandPool GetHandle() const
    {
        return handle_;
    }

private:
    VulkanDevice* device_ = nullptr;
    VkCommandPool handle_ = VK_NULL_HANDLE;
};
