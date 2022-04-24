#pragma once
#include "vulkan/vulkan_core.h"

#include "VulkanQueue.h"

class VulkanContext;

struct QueueFamilyIndices
{
    uint32_t graphics = -1;
    uint32_t compute = -1;
    uint32_t transfer = -1;
};

struct DeviceCreationProperties
{
    VkPhysicalDeviceFeatures features;  // Features that should be enabled
    std::vector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };  // Extensions that should be enabled
    VkQueueFlags requested_queue_types = VK_QUEUE_GRAPHICS_BIT; // Bit flags specifying which queue types the device has to support
    bool is_headless = false;   // Set to true if we don't need swapchain extension support (e.g. off-screen rendering)
    VkSurfaceKHR surface = VK_NULL_HANDLE;  // Surface to check for swapchain support if required
    const std::vector<const char*>& valiation_layers = { "VK_LAYER_KHRONOS_validation" };
};

// Wrapper around both physical and logical Vulkan device
class VulkanDevice 
{
public:
    VulkanDevice(VulkanContext* RHI, VkPhysicalDevice physical_device, int32 device_idx);
    ~VulkanDevice();

    void Destroy();

    void QueryGPUInfo();

    bool IsDiscrete() const
    {
        return is_discrete_;
    }

    void QueryPhysicalDeviceFeatures();
    void QuerySupportedDeviceExtensions();
    void QuerySupportedDeviceValidationLayers();

    int32 GetDeviceIndex() const
    {
        return device_idx_;
    }

    /**
    * Create logical device based on the physical device.
    *
    * @param device_creation_props DeviceCreationProperties struct defining the settings to create the device
    * @return VkResult of the vk API call to create the device
    */
    void CreateLogicalDevice();

    /**
    * Check if extensions with the given name are supported by the physical device
    *
    * @param extensions vector of extension names
    * @return True if supported, false if nah.
    */
    bool AreExtensionsSupported(const std::vector<const char*>& extensions) const;

    /**
    *    Wrapper around vkDeviceWaitIdle.
    */
    void WaitUntilIdle();

    /**
    * Set up the present queue.
    * This is done separatly, as we need access to the surface before we can check the present support.
    * 
    * @param surface Handle to the surface.
    */
    void InitPresentQueue(VkSurfaceKHR surface);

    VkPhysicalDevice GetPhysicalDeviceHandle() const
    {
        return physical_device_;
    }

    const VkPhysicalDeviceProperties& GetPhysicalDeviceProperties() const
    {
        return physical_device_properties_;
    }

    VulkanQueue* GetGraphicsQueue()
    {
        return graphics_queue_;
    }

    VulkanQueue* GetComputeQueue()
    {
        return compute_queue_;
    }

    VulkanQueue* GetTransferQueue()
    {
        return transfer_queue_;
    }

    VulkanQueue* GetPresentQueue()
    {
        return present_queue_;
    }

    VkDevice GetLogicalDeviceHandle() const
    {
        return logical_device_;
    }

    // Queries the physical device for desired formats and returns the first one that's supported.
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    VkFormat FindDepthFormat();

    /**
     * Retrieve max number of supported samples from the physical device.
     * Takes into account both color and depth samples.
     */
    VkSampleCountFlagBits GetMaxNumSamples()
    {
        const VkPhysicalDeviceProperties& physical_device_properties = GetPhysicalDeviceProperties();

        // We use depth buffering, so we have to account for both color and depth samples
        VkSampleCountFlags sample_count_flags = physical_device_properties.limits.framebufferColorSampleCounts & physical_device_properties.limits.framebufferDepthSampleCounts;
        if (sample_count_flags & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (sample_count_flags & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (sample_count_flags & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (sample_count_flags & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
        if (sample_count_flags & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
        if (sample_count_flags & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

        return VK_SAMPLE_COUNT_1_BIT;
    }

private:
    std::vector<const char*> GetRequiredExtensions() const;
    std::vector<const char*> GetRequiredValidationLayers() const;
    QueueFamilyIndices GetQueueFamilyIndices(uint32_t requested_family_flags) const;

    VulkanContext* RHI_ = nullptr;
    int32 device_idx_ = -1;
    bool is_discrete_ = false;

    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE; // We do not have to clean this up manually
    VkPhysicalDeviceProperties physical_device_properties_{};  // basic details, e.g. name, type and supported Vulkan version
    VkPhysicalDeviceFeatures physical_device_features_{};   // supported optional features, e.g. texture compression,
                                                            // 64 bit floats and multi viewport rendering (useful for VR)
    std::vector<VkExtensionProperties> supported_extensions_;
    std::vector<VkLayerProperties> supported_validation_layers_;

    VkDevice logical_device_ = VK_NULL_HANDLE;

    VulkanQueue* graphics_queue_ = nullptr;
    VulkanQueue* compute_queue_ = nullptr;
    VulkanQueue* transfer_queue_ = nullptr;
    VulkanQueue* present_queue_ = nullptr;

    // GPU Info
    std::vector<VkQueueFamilyProperties> queue_family_properties_;
};
