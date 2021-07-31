#pragma once
#include "vulkan/vulkan_core.h"

// Wrapper around Vulkan buffers backed by device memory
struct Buffer
{
    VkDevice device_;
    VkDeviceMemory memory_handle_ = VK_NULL_HANDLE;
    VkBuffer buffer_handle_ = VK_NULL_HANDLE;
    
    void Destroy();
    
    VkResult Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void* GetMapped();
    void Unmap();
    
private:
    void* mapped_addr_ = nullptr;
};
