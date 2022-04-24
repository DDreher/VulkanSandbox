#pragma once
#include "vulkan/vulkan_core.h"

#include "VulkanDevice.h"

class VulkanMemory
{
public:
    VulkanMemory(VulkanDevice* device, VkMemoryRequirements mem_requirements, VkMemoryPropertyFlags mem_properties, VkMemoryAllocateFlags mem_allocate_flags = {});
    ~VulkanMemory();

    void Upload(const void* data, size_t size);

    void* Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void Unmap();

    VkDeviceMemory GetHandle() const { return memory_handle_; }

    /**
     * GPU may offer different types of memory which differ in terms of allowed operations or performance.
     * This function helps to find the available memory which suits our needs best.
     */
    static uint32 FindMemoryType(VulkanDevice* device, uint32 type_filter, VkMemoryPropertyFlags mem_properties);

private:
    VulkanDevice* device_ = nullptr;
    VkDeviceMemory memory_handle_ = VK_NULL_HANDLE;
};
