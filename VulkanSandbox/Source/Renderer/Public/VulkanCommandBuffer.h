#pragma once
#include "vulkan/vulkan_core.h"

#include "VulkanDevice.h"
#include "VulkanCommandBufferPool.h"

class VulkanCommandBuffer
{
public:
    VulkanCommandBuffer(VulkanDevice* device, VulkanCommandBufferPool* command_buffer_pool, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    ~VulkanCommandBuffer();

    inline const VkCommandBuffer& GetHandle() const
    {
        return handle_;
    }

private:
    VulkanDevice* device_ = nullptr;
    VulkanCommandBufferPool* command_buffer_pool_ = nullptr;
    VkCommandBuffer handle_ = VK_NULL_HANDLE;
};
