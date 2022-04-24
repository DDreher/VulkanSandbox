#include "VulkanMemory.h"
#include "VulkanMacros.h"


VulkanMemory::VulkanMemory(VulkanDevice* device, VkMemoryRequirements mem_requirements, VkMemoryPropertyFlags mem_properties,
    VkMemoryAllocateFlags mem_allocate_flags /*= {}*/) : device_(device)
{
    CHECK(device != nullptr);

    VkMemoryAllocateFlagsInfo mem_flags_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
    mem_flags_info.pNext = nullptr;
    mem_flags_info.flags = mem_allocate_flags;

    VkMemoryAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = VulkanMemory::FindMemoryType(device, mem_requirements.memoryTypeBits, mem_properties);
    alloc_info.pNext = &mem_flags_info;

    if (vkAllocateMemory(device->GetLogicalDeviceHandle(), &alloc_info, nullptr, &memory_handle_) != VK_SUCCESS)
    {
        LOG_ERROR("Failed to allocate memory on GPU!");
        exit(EXIT_FAILURE);
    }
}

VulkanMemory::~VulkanMemory()
{
    CHECK(memory_handle_ != VK_NULL_HANDLE);
    vkFreeMemory(device_->GetLogicalDeviceHandle(), memory_handle_, nullptr);
    memory_handle_ = VK_NULL_HANDLE;
}

void VulkanMemory::Upload(const void* data, VkDeviceSize size)
{

}

void* VulkanMemory::Map(VkDeviceSize size, VkDeviceSize offset)
{
    CHECK(device_ != nullptr);
    CHECK(memory_handle_ != VK_NULL_HANDLE);
    CHECK(size != 0);

    static const VkMemoryMapFlags FLAGS = 0;
    void* mapped_addr = nullptr;
    VERIFY_VK_RESULT_MSG(vkMapMemory(device_->GetLogicalDeviceHandle(), memory_handle_, offset, size, FLAGS, &mapped_addr), "Failed to map memory!");

    return mapped_addr;
}

void VulkanMemory::Unmap()
{
    CHECK(device_ != nullptr);
    CHECK(memory_handle_ != VK_NULL_HANDLE);
    vkUnmapMemory(device_->GetLogicalDeviceHandle(), memory_handle_);
}

uint32 VulkanMemory::FindMemoryType(VulkanDevice* device, uint32 type_filter, VkMemoryPropertyFlags mem_properties)
{
    // First query info about available memory types of the physical device
    // VkPhysicalDeviceMemoryProperties::memoryHeaps -> distinct memory resources (e.g. dedicated VRAM or swap space in RAM when VRAM is depleted)
    // VkPhysicalDeviceMemoryProperties::memoryTypes -> types which exist inside the memoryHeaps.
    VkPhysicalDeviceMemoryProperties physical_device_mem_properties;
    vkGetPhysicalDeviceMemoryProperties(device->GetPhysicalDeviceHandle(), &physical_device_mem_properties);

    // Then find a memory type that is suitable for the buffer itself
    for (uint32 i = 0; i < physical_device_mem_properties.memoryTypeCount; i++)
    {
        // We simply check if the bit is set for the memory types we want to accept
        bool is_type_accepted = type_filter & (1 << i);
        if (is_type_accepted)
        {
            // We also have to check support for all the requested mem properties!
            // e.g., writing from CPU to VRAM requires mapping to host, etc.
            bool are_required_properties_supported = (physical_device_mem_properties.memoryTypes[i].propertyFlags & mem_properties) == mem_properties;
            if (are_required_properties_supported)
            {
                return i;
            }
        }
    }

    // Welp, we're screwed.
    LOG_ERROR("Failed to find suitable memory type!");
    exit(EXIT_FAILURE);

    return 0;
}
