#pragma once
#include "vulkan/vulkan_core.h"

class VulkanQueue;
class VulkanCommandBufferPool;
class VulkanDevice;

/*
 * Wrapper around vkCommandBuffer.
 *
 * Drawing operations and memory transfers are stored in command buffers. These are retrieved from command pools.
 * We can fill these buffers in multiple threads and then execute them all at once on the render thread.
 */
class VulkanCommandBuffer
{
public:
    VulkanCommandBuffer(VulkanDevice* device, VulkanCommandBufferPool* command_buffer_pool,
        VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    ~VulkanCommandBuffer();

    VkResult Begin(const VkCommandBufferUsageFlags flags);
    VkResult End();
    VkResult Reset();

    inline const VkCommandBuffer& GetHandle() const
    {
        return handle_;
    }

    bool IsRecording() const
    {
        return is_recording_;
    }

private:
    VulkanDevice* device_ = nullptr;
    VulkanCommandBufferPool* command_buffer_pool_ = nullptr;
    VkCommandBuffer handle_ = VK_NULL_HANDLE;

    bool is_recording_ = false;
    bool is_submitted_ = false;
};

/*
 * Wrapper around VkCommandPool.
 * Used to allocate command buffers for a given queue.
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

    void DestroyCommandBuffer(const VkCommandBuffer* command_buffer);
private:
    VulkanDevice* device_ = nullptr;
    VkCommandPool handle_ = VK_NULL_HANDLE;
};
