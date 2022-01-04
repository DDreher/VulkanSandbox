#include "VulkanRHI.h"

#include "vulkan/vulkan_core.h"

#include "VulkanMacros.h"

VulkanRHI::VulkanRHI()
{
}

void VulkanRHI::Init()
{
    instance_.Init();
    SelectAndInitDevice();
}

void VulkanRHI::Shutdown()
{
    for(size_t i=0; i<found_devices_.size(); ++i)
    {
        CHECK(found_devices_[i] != nullptr);
        found_devices_[i]->Destroy();
        delete found_devices_[i];
        found_devices_[i] = nullptr;
    }
    found_devices_.clear();

    device_ = nullptr;

    instance_.Shutdown();
}

VkImageView VulkanRHI::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t num_mips)
{
    CHECK(device_ != nullptr);
    CHECK(device_->GetLogicalDeviceHandle() != VK_NULL_HANDLE);

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = num_mips;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    VkResult result = vkCreateImageView(device_->GetLogicalDeviceHandle(), &view_info, nullptr, &image_view);
    VERIFY_VK_RESULT(result);

    if (result != VK_SUCCESS)
    {
        LOG_ERROR("Failed to create image view!");
        exit(EXIT_FAILURE);
    }

    return image_view;
}

void VulkanRHI::CreateImage(uint32 width, uint32 height, uint32 num_mips, VkSampleCountFlagBits num_samples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& image_memory)
{
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = static_cast<uint32_t>(width);
    image_info.extent.height = static_cast<uint32_t>(height);
    image_info.extent.depth = 1; // One color value per texel
    image_info.mipLevels = num_mips;
    image_info.arrayLayers = 1;  // Single texture, no texture array
    image_info.format = format;
    image_info.tiling = tiling; // VK_IMAGE_TILING_LINEAR -> Texels are laid out in row-major order like the tex_data array
                                // -> If we want to access texels directly in the memory, we have to use this!
                                // VK_IMAGE_TILING_OPTIMAL -> Texels are laid out in an implementation defined order for optimal access
                                // -> We use this here, because we use a staging buffer instead of a staging image
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;   // VK_IMAGE_LAYOUT_UNDEFINED -> Not usable by the GPU and the very first transition will discard the texels
                                                            // VK_IMAGE_LAYOUT_PREINITIALIZED -> Not usable by the GPU, but the first transition will preserve the texels.
                                                            // Useful if we want to use an image as a staging image, e.g. upload data to it and then transition the image to be
                                                            // a transfer source while preserving the data.
                                                            // In our case, we transition the image to be a transfer destination and then copy the texel data to it from a buffer.
    image_info.usage = usage;    // We want to transfer data to this image and we want to access the image in the shader
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Image is only used by the graphics queue family
    image_info.samples = num_samples; // Related to multisampling. Only needed if image is used as attachment.
    image_info.flags = 0; // Optional. Related to sparse images.

    if (vkCreateImage(device_->GetLogicalDeviceHandle(), &image_info, nullptr, &image) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image!");
    }

    // Allocate memory for the image - Similar to allocating memory for a buffer
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device_->GetLogicalDeviceHandle(), image, &mem_requirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = mem_requirements.size;
    allocInfo.memoryTypeIndex = GetDevice()->FindMemoryType(mem_requirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device_->GetLogicalDeviceHandle(), &allocInfo, nullptr, &image_memory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate image memory!");
    }

    vkBindImageMemory(device_->GetLogicalDeviceHandle(), image, image_memory, 0);
}

void VulkanRHI::SelectAndInitDevice()
{
    uint32 gpu_count = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance_.GetHandle(), &gpu_count, nullptr);
    CHECK_MSG(gpu_count > 0, "Failed to find GPU / driver with Vulkan support!");

    std::vector<VkPhysicalDevice> physical_devices(gpu_count);
    VERIFY_VK_RESULT(vkEnumeratePhysicalDevices(instance_.GetHandle(), &gpu_count, physical_devices.data()));
    LOG("Found {} GPU(s)", gpu_count);

    // Create device objects and find the GPU that fits our needs best
    for(uint32 i = 0; i<gpu_count; ++i)
    {
        const VkPhysicalDevice& device_handle = physical_devices[i];
        VulkanDevice* device = new VulkanDevice(this, device_handle, i);
        found_devices_.push_back(device);
        device->QueryGPUInfo();

        if(device->IsDiscrete())
        {
            // For now we'll just use the first discrete GPU we find.
            // We could also add more complicated logic here (check GPU vendors, etc) but for this small application that would be overkill...
            device_ = device;
            break;
        }
    }

    if(device_ == nullptr)
    {
        // As a last resort we'll just use the first GPU we found.
        LOG_WARN("Could not find discrete GPU! Using any other GPU instead.");
        device_ = found_devices_[0];
    }

    if(device_ == nullptr)
    {
        LOG_ERROR("Failed to find a suitable device!");
        exit(EXIT_FAILURE);
    }

    LOG("Using device: {}", device_->GetDeviceIndex());
    device_->CreateLogicalDevice();
}
