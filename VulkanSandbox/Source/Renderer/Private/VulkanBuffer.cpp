#include "VulkanBuffer.h"

#include "VulkanDevice.h"

VulkanBuffer VulkanBuffer::Create(VulkanDevice* device, const VkDeviceSize& size, const VkBufferUsageFlags& usage, const VkMemoryPropertyFlags& properties)
{
    VulkanBuffer out_buffer;
    out_buffer.device_ = device;
    out_buffer.size = size;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage; // Specify how the buffer is used. Can be multiple with bitwise or.
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;    // Buffers can be owned by specific queue families or shared between multiple queue families.
                                                            // For now this buffer will only be used by the graphics queue, so we use exclusive access.
    buffer_info.flags = 0;  // Used to configure sparse buffer memory (not relevant for us right now)

    if (vkCreateBuffer(device->GetLogicalDeviceHandle(), &buffer_info, nullptr, &out_buffer.buffer_handle_) != VK_SUCCESS)
    {
        LOG_ERROR("Failed to create buffer");
        exit(EXIT_FAILURE);
    }

    // Buffer was created, but no memory has been allocated yet.
    // We have to do this ourselves!

    // First query memory requirements.
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device->GetLogicalDeviceHandle(), out_buffer.buffer_handle_, &mem_requirements);

    // Then allocate the memory
    // NOTE: In a real application, we shouldn't allocate memory for every single resource we create. (inefficient / max num of simultaneous mem allocations is limited)
    // Instead we should allocate a large chunk of memory and then split it up with the offset parameters by using a custom allocator.
    // See https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator for examples
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = device->FindMemoryType(mem_requirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device->GetLogicalDeviceHandle(), &alloc_info, nullptr, &out_buffer.memory_handle_) != VK_SUCCESS)
    {
        LOG_ERROR("Failed to allocate vertex buffer memory!");
        exit(EXIT_FAILURE);
    }

    // Finally associate the allocated memory with the buffer
    vkBindBufferMemory(device->GetLogicalDeviceHandle(), out_buffer.buffer_handle_, out_buffer.memory_handle_, 0 /*offset within the memory*/);

    return out_buffer;
}

void VulkanBuffer::Destroy()
{
    CHECK(device_ != nullptr);
    CHECK(buffer_handle_!= VK_NULL_HANDLE);
    CHECK(memory_handle_ != VK_NULL_HANDLE);

    vkDestroyBuffer(device_->GetLogicalDeviceHandle(), buffer_handle_, nullptr);
    vkFreeMemory(device_->GetLogicalDeviceHandle(), memory_handle_, nullptr);
}

void* VulkanBuffer::Map(VkDeviceSize size, VkDeviceSize offset)
{
    CHECK(device_ != VK_NULL_HANDLE);
    CHECK(memory_handle_ != VK_NULL_HANDLE);
    CHECK(size != 0);
    static const VkMemoryMapFlags FLAGS = 0;

    void* mapped_addr = nullptr;
    CHECK(vkMapMemory(device_->GetLogicalDeviceHandle(), memory_handle_, offset, size, FLAGS, &mapped_addr) == VK_SUCCESS);

    return mapped_addr;
}

void VulkanBuffer::Unmap()
{
    CHECK(device_ != nullptr);
    CHECK(memory_handle_ != VK_NULL_HANDLE);
    vkUnmapMemory(device_->GetLogicalDeviceHandle(), memory_handle_);
}

uint32 VulkanBuffer::FindMemoryType(VulkanDevice* device, uint32 type_filter, VkMemoryPropertyFlags properties)
{
    // GPU may offer different types of memory which differ in terms of allowed operations or performance.
    // This function helps to find the available memory which suits our needs best.

    // First query info about available memory types of the physical device
    // VkPhysicalDeviceMemoryProperties::memoryHeaps -> distinct memory resources (e.g. dedicated VRAM or swap space in RAM when VRAM is depleted)
    // VkPhysicalDeviceMemoryProperties::memoryTypes -> types which exist inside the memoryHeaps.
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(device->GetPhysicalDeviceHandle(), &mem_properties);

    // Then find a memory type that is suitable for the buffer itself
    for (uint32 i = 0; i < mem_properties.memoryTypeCount; i++)
    {
        // type_filer specifies the bit field of memory types that are suitable
        // -> We simply check if the bit is set for the memory types we want to accept
        bool is_type_accepted = type_filter & (1 << i);
        if (is_type_accepted)
        {
            // We also have to check for the properties of the memory!
            // For example, we may want to be able to write to a vertex buffer from the CPU, so it has to support
            // being mapped to the host.
            // We may have multiple requested properties, so we have to use a bitwise AND operation to check if ALL properties are
            // supported.
            bool are_required_properties_supported = (mem_properties.memoryTypes[i].propertyFlags & properties) == properties;
            if (are_required_properties_supported)
            {
                return i;
            }
        }
    }

    // Welp, we're screwed.
    CHECK(false);
    LOG_ERROR("Failed to find suitable memory type!");
    exit(EXIT_FAILURE);
    return -1;
}