#pragma once
#include "SDL_vulkan.h"
#include "SDL_video.h"
#include <vulkan/vulkan_core.h>

#include "VulkanCommandBuffer.h"
#include "VulkanDevice.h"
#include "VulkanInstance.h"

/**
 * Owner of all application specific Vulkan resources
 */
class VulkanContext
{
public:
    VulkanContext();
    ~VulkanContext() {};

    static VulkanContext& Get();

    /**
    *    Initializes the RHI using Vulkan as backend.
    *    Creates the Vulkan Instance and selects the physical and logical devices to use.
    */
    void Init(SDL_Window* window);

    /**
    *    Cleans up the RHI.
    *    Destroys devices.
    */
    void Shutdown();

    void CreateImage(uint32 width, uint32 height, uint32 num_mips, VkSampleCountFlagBits num_samples, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& image_memory);

    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t num_mips);

    const std::vector<char*>& GetInstanceExtensions() const
    {
        return instance_extensions_;
    }

    const std::vector<char*>& GetInstanceLayers() const
    {
        return instance_layers_;
    }

    const VulkanInstance& GetInstance() const
    {
        return instance_;
    }

    VulkanDevice* GetDevice()
    {
        return device_;
    }

    void CreateSurface(SDL_Window* window);

    const VkSurfaceKHR GetSurface() const
    {
        return surface_;
    }

    const bool IsInitialized() const
    {
        return is_initialized;
    }

private:
    bool is_initialized = false;

    void SelectAndInitDevice();

    VulkanInstance instance_;
    
    std::vector<char*> instance_extensions_;
    std::vector<char*> instance_layers_;

    std::vector<VulkanDevice*> found_devices_;
    VulkanDevice* device_ = nullptr;

    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
};
