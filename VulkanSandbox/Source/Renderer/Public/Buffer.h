#pragma once
#include "vulkan/vulkan_core.h"

// Wrapper around Vulkan buffers backed by device memory
struct Buffer
{
    VkDevice device_;
    VkDeviceMemory memory_handle_ = VK_NULL_HANDLE;
    VkBuffer buffer_handle_ = VK_NULL_HANDLE;
    uint32_t size = 0;
    
    void Destroy();
    
    void* Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void Unmap();
};
