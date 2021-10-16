#include "Buffer.h"

#include "assert.h"

void Buffer::Destroy()
{
    assert(device_ != VK_NULL_HANDLE);
    assert(buffer_handle_!= VK_NULL_HANDLE);
    assert(memory_handle_ != VK_NULL_HANDLE);

    vkDestroyBuffer(device_, buffer_handle_, nullptr);
    vkFreeMemory(device_, memory_handle_, nullptr);
}

void* Buffer::Map(VkDeviceSize size, VkDeviceSize offset)
{
    assert(device_ != VK_NULL_HANDLE);
    assert(memory_handle_ != VK_NULL_HANDLE);
    assert(size != 0);
    static const VkMemoryMapFlags FLAGS = 0;

    void* mapped_addr = nullptr;
    assert(vkMapMemory(device_, memory_handle_, offset, size, FLAGS, &mapped_addr) == VK_SUCCESS);

    return mapped_addr;
}

void Buffer::Unmap()
{
    assert(device_ != VK_NULL_HANDLE);
    assert(memory_handle_ != VK_NULL_HANDLE);
    vkUnmapMemory(device_, memory_handle_);
}
