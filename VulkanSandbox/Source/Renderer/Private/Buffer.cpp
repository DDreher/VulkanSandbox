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

VkResult Buffer::Map(VkDeviceSize size, VkDeviceSize offset)
{
    assert(device_ != VK_NULL_HANDLE);
    assert(memory_handle_ != VK_NULL_HANDLE);
    assert(size != 0);
    assert(mapped_addr_ == nullptr);
    static const VkMemoryMapFlags FLAGS = 0;
    return vkMapMemory(device_, memory_handle_, offset, size, FLAGS, &mapped_addr_);
}

void* Buffer::GetMapped()
{
    assert(mapped_addr_ != nullptr);
    return mapped_addr_;
}

void Buffer::Unmap()
{
    assert(device_ != VK_NULL_HANDLE);
    assert(memory_handle_ != VK_NULL_HANDLE);
    assert(mapped_addr_ != nullptr);
    vkUnmapMemory(device_, memory_handle_);
    mapped_addr_ = nullptr;
}
