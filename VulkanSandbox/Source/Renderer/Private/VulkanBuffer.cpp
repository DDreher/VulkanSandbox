#include "VulkanBuffer.h"

#include <vulkan/vulkan.hpp>

#include "VulkanDevice.h"
#include "VulkanMacros.h"
#include "VulkanMemory.h"

VulkanBuffer::VulkanBuffer(VulkanDevice* device, const VkDeviceSize& size, const VkBufferUsageFlags& usage, const VkMemoryPropertyFlags& properties) :
    device_(device), size_(size)
{
    CHECK(device != nullptr);
    CHECK(size != 0);

    VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_info.size = size;
    buffer_info.usage = usage; // Can be multiple with bitwise or.
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;    // exclusive -> owned by specific queue family
                                                            // shared -> used by multiple queue families.
    buffer_info.flags = 0;  // Used to configure sparse buffer memory (not relevant for us right now)

    if (vkCreateBuffer(device->GetLogicalDeviceHandle(), &buffer_info, nullptr, &buffer_handle_) != VK_SUCCESS)
    {
        LOG_ERROR("Failed to create buffer");
        exit(EXIT_FAILURE);
    }

    // Buffer was created, but no memory has been allocated yet.
    // We have to do this ourselves!
    memory_ = MakeShared<VulkanMemory>(device, GetMemoryRequirements(), properties);

    // Finally associate the allocated memory with the buffer
    VERIFY_VK_RESULT_MSG(vkBindBufferMemory(device->GetLogicalDeviceHandle(), buffer_handle_, memory_->GetHandle(), 0 /*offset within the memory*/),
        "Failed to bind buffer to memory");
}

VulkanBuffer::~VulkanBuffer()
{
    CHECK(device_ != nullptr);
    CHECK(buffer_handle_!= VK_NULL_HANDLE);

    vkDestroyBuffer(device_->GetLogicalDeviceHandle(), buffer_handle_, nullptr);

    memory_.reset();
}

void* VulkanBuffer::Map(VkDeviceSize size, VkDeviceSize offset)
{
    CHECK(device_ != nullptr);
    CHECK(memory_ != nullptr);
    CHECK(size != 0);

    return memory_->Map(size, offset);
}

void VulkanBuffer::Unmap()
{
    CHECK(device_ != nullptr);
    CHECK(memory_ != nullptr);

    memory_->Unmap();
}

VkMemoryRequirements VulkanBuffer::GetMemoryRequirements()
{
    // First query memory requirements.
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device_->GetLogicalDeviceHandle(), buffer_handle_, &mem_requirements);
    return mem_requirements;
}
