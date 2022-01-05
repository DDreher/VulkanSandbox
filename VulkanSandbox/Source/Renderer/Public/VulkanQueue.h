#pragma once
#include <vulkan/vulkan_core.h>

#include "VulkanCommandBuffer.h"

class VulkanDevice;

class VulkanQueue
{
public:
    VulkanQueue(VulkanDevice* device, uint32 family_idx);
    ~VulkanQueue();

    void Submit(const VulkanCommandBuffer& command_buffer) const;

    VkQueue GetHandle() const
    {
        return queue_handle_;
    }

    uint32 GetFamilyIndex() const
    {
        return family_idx_;
    }

    uint32 GetQueueIndex() const
    {
        return queue_idx_;
    }

private:
    VkQueue queue_handle_ = VK_NULL_HANDLE; // We do not have to clean this up manually, clean up of logical device takes care of this.
    uint32 family_idx_ = 0;
    uint32 queue_idx_ = 0;
    VulkanDevice* device_ = nullptr;
};
