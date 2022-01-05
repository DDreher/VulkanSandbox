#include "VulkanImage.h"

#include "VulkanDevice.h"
#include "VulkanMacros.h"

VulkanImage::VulkanImage(VulkanDevice* device, uint32 width, uint32 height, uint32 num_layers, uint32 num_mips, VkFormat format, VkImageLayout layout,
    VkSampleCountFlagBits num_samples, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageViewType view_type, VkImageAspectFlags view_aspect_flags)
    : device_(device),
    width_(width),
    height_(height),
    num_layers_(num_layers),
    num_mips_(num_mips),
    format_(format),
    layout_(layout),
    num_samples_(num_samples),
    tiling_(tiling),
    usage_(usage),
    mem_properties_(properties)
{
    image_ = VulkanImage::CreateImage(device, width, height, num_mips, num_layers, num_samples, format, tiling, usage);
    image_memory_ = VulkanImage::AllocateImageMemory(device, image_, properties);
    view_ = VulkanImage::CreateImageView(device, image_, format, num_mips, num_layers, view_type, view_aspect_flags);
}

VulkanImage::~VulkanImage()
{

}

void VulkanImage::Destroy()
{
    vkDestroyImageView(device_->GetLogicalDeviceHandle(), view_, nullptr);
    vkDestroyImage(device_->GetLogicalDeviceHandle(), image_, nullptr);
    vkFreeMemory(device_->GetLogicalDeviceHandle(), image_memory_, nullptr);
}

VkImage VulkanImage::CreateImage(VulkanDevice* device, uint32 width, uint32 height, uint32 num_mips, uint32 num_layers, VkSampleCountFlagBits num_samples,
    VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage)
{
    CHECK(device != nullptr);
    CHECK(device->GetLogicalDeviceHandle() != VK_NULL_HANDLE);

    VkImageCreateInfo image_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1; // One color value per texel
    image_info.mipLevels = num_mips;
    image_info.arrayLayers = num_layers; // 1 if normal texture, > 1 if texture array
    image_info.format = format;
    image_info.tiling = tiling; // VK_IMAGE_TILING_LINEAR -> Texels are laid out in row-major order like the tex_data array
                                // -> If we want to access texels directly in the memory, we have to use this!
                                // VK_IMAGE_TILING_OPTIMAL -> Texels are laid out in an implementation defined order for optimal access
                                // -> We use this here, because we use a staging buffer instead of a staging image
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;   // VK_IMAGE_LAYOUT_UNDEFINED -> Not usable by the GPU. First transition will discard texels.
                                                            // VK_IMAGE_LAYOUT_PREINITIALIZED -> Not usable by the GPU. First transition will preserve texels.
                                                            // Useful to use an image as a staging image, e.g. upload data and then transition the image to be
                                                            // a transfer source while preserving the data.
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Image is only used by the graphics queue family
    image_info.samples = num_samples; // Related to multisampling. Only needed if image is used as attachment.
    image_info.flags = 0; // Optional. Related to sparse images.

    VkImage image_handle;
    VERIFY_VK_RESULT(vkCreateImage(device->GetLogicalDeviceHandle(), &image_info, nullptr, &image_handle));
    return image_handle;
}

VkDeviceMemory VulkanImage::AllocateImageMemory(VulkanDevice* device, VkImage image_handle, VkMemoryPropertyFlags properties)
{
    CHECK(device != nullptr);
    CHECK(device->GetLogicalDeviceHandle() != VK_NULL_HANDLE);

    // Allocate memory for an image - Similar to allocating memory for a buffer
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device->GetLogicalDeviceHandle(), image_handle, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = device->FindMemoryType(mem_requirements.memoryTypeBits, properties);

    VkDeviceMemory mem_handle;
    VERIFY_VK_RESULT(vkAllocateMemory(device->GetLogicalDeviceHandle(), &alloc_info, nullptr, &mem_handle));
    VERIFY_VK_RESULT(vkBindImageMemory(device->GetLogicalDeviceHandle(), image_handle, mem_handle, 0));

    return mem_handle;
}

VkImageView VulkanImage::CreateImageView(VulkanDevice* device, VkImage image_handle, VkFormat format, uint32 num_mips, uint32 num_layers,
    VkImageViewType view_type, VkImageAspectFlags aspect_flags)
{
    CHECK(device != nullptr);
    CHECK(device->GetLogicalDeviceHandle() != VK_NULL_HANDLE);

    VkImageViewCreateInfo create_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    create_info.image = image_handle;
    create_info.viewType = view_type;
    create_info.format = format;
    create_info.subresourceRange.aspectMask = aspect_flags;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = num_mips;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = num_layers;

    VkImageView image_view;
    VERIFY_VK_RESULT(vkCreateImageView(device->GetLogicalDeviceHandle(), &create_info, nullptr, &image_view));
    return image_view;
}
