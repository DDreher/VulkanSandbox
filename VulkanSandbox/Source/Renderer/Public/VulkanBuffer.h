#pragma once
#include "vulkan/vulkan_core.h"

class VulkanDevice;

// Wrapper around Vulkan buffers backed by device memory
class VulkanBuffer
{
public:
    static VulkanBuffer Create(VulkanDevice* device, const VkDeviceSize& size, const VkBufferUsageFlags& usage, const VkMemoryPropertyFlags& properties);
    void Destroy();
    void* Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void Unmap();

    VkBuffer GetBufferHandle() const
    {
        return buffer_handle_;
    }

    VkDeviceMemory GetDeviceMemoryHandle() const
    {
        return memory_handle_;
    }

private:
    static uint32 FindMemoryType(VulkanDevice* device, uint32 type_filter, VkMemoryPropertyFlags properties);

    VulkanDevice* device_ = nullptr;
    VkDeviceMemory memory_handle_ = VK_NULL_HANDLE;
    VkBuffer buffer_handle_ = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};
