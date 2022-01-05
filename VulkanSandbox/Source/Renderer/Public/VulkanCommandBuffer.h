#pragma once
#include "vulkan/vulkan_core.h"

class VulkanQueue;
class VulkanCommandBufferPool;
class VulkanDevice;

class VulkanCommandBuffer
{
public:
    VulkanCommandBuffer(VulkanDevice* device, VulkanCommandBufferPool* command_buffer_pool,
        VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    ~VulkanCommandBuffer();

    void Begin(const VkCommandBufferUsageFlags& flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    void End();

    inline const VkCommandBuffer& GetHandle() const
    {
        return handle_;
    }

    bool IsInProgress() const
    {
        return is_record_in_progress;
    }

private:
    VulkanDevice* device_ = nullptr;
    VulkanCommandBufferPool* command_buffer_pool_ = nullptr;
    VkCommandBuffer handle_ = VK_NULL_HANDLE;

    bool is_record_in_progress = false;
};

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
    VulkanCommandBufferPool(VulkanDevice* device, VulkanQueue* queue, const VkCommandPoolCreateFlags& flags = 0);
    ~VulkanCommandBufferPool();

    inline VkCommandPool GetHandle() const
    {
        return handle_;
    }

    VulkanCommandBuffer CreateCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY)
    {
        return VulkanCommandBuffer(device_, this, level);
    }

private:
    VulkanDevice* device_ = nullptr;
    VkCommandPool handle_ = VK_NULL_HANDLE;
};
