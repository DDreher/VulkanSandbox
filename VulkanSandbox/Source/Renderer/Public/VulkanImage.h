#pragma once
#include "vulkan/vulkan_core.h"

#include "VulkanDevice.h"

class VulkanImage
{
public:
    VulkanImage(VulkanDevice* device, uint32 width, uint32 height, uint32 num_layers, uint32 num_mips, VkFormat format, VkImageLayout layout, VkSampleCountFlagBits num_samples, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageViewType view_type, VkImageAspectFlags view_aspect_flags);

    ~VulkanImage();

    void Destroy();

    static VkImage CreateImage(VulkanDevice* device, uint32 width, uint32 height, uint32 num_mips, uint32 num_layers, VkSampleCountFlagBits num_samples,
        VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage);

    static VkDeviceMemory AllocateImageMemory(VulkanDevice* device, VkImage image_handle, VkMemoryPropertyFlags properties);

    static VkImageView CreateImageView(VulkanDevice* device, VkImage image_handle, VkFormat format, uint32 num_mips, uint32 num_layers, VkImageViewType view_type, VkImageAspectFlags aspect_flags);

private:
    VulkanDevice* device_ = nullptr;
    VkImage image_;
    VkImageView view_;
    VkDeviceMemory image_memory_;

    VkFormat format_;
    VkImageLayout layout_;
    uint32 width_ = 0;
    uint32 height_ = 0;
    uint32 num_layers_ = 0;
    uint32 num_mips_ = 0;
    uint32 num_samples_ = 0;
    VkImageTiling tiling_;
    VkImageUsageFlags usage_;
    VkMemoryPropertyFlags mem_properties_;
};
