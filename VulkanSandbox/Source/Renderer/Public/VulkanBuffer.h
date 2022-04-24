#pragma once
#include "vulkan/vulkan_core.h"

#include "VulkanMemory.h"

class VulkanDevice;

// Wrapper around Vulkan buffers backed by device memory
class VulkanBuffer
{
public:
    VulkanBuffer(VulkanDevice* device, const VkDeviceSize& size, const VkBufferUsageFlags& usage, const VkMemoryPropertyFlags& properties);
    ~VulkanBuffer();

    void* Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void Unmap();

    VkBuffer GetBufferHandle() const
    {
        return buffer_handle_;
    }

    SharedPtr<VulkanMemory> GetMemory() const
    {
        return memory_;
    }


private:
    VkMemoryRequirements GetMemoryRequirements();

    VulkanDevice* device_ = nullptr;
    VkBuffer buffer_handle_ = VK_NULL_HANDLE;
    SharedPtr<VulkanMemory> memory_;
    VkDeviceSize size_ = 0;
};
