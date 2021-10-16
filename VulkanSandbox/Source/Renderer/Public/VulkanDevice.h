#pragma once
#include "vulkan/vulkan_core.h"

class VulkanRHI;
class VulkanQueue;

struct QueueFamilyIndices
{
    int32 graphics_family = -1;
    int32 present_family = -1;
    int32 transfer_family = -1;
    int32 compute_family = -1;
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
    VulkanDevice(VulkanRHI* RHI, VkPhysicalDevice physical_device, int32 device_idx);
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
    * Try to find queue family indices for graphics, present, transfer and compute queue family. 
    * Indices of different queue families may overlap.
    * The graphics queue family preferably also supports presenting. Otherwise search for a dedicated present family.
    * Transfer and compute queue families are preferably dedicated.
    * 
    * @param surface The surface to check for swapchain support if required
    * @return QueueFamilyIndices struct wrapping the found indices.
    */
    QueueFamilyIndices FindQueueFamilies(VkSurfaceKHR surface) const;

    /**
    *    Wrapper around vkDeviceWaitIdle.
    */
    void WaitUntilIdle();

    VkPhysicalDevice GetPhysicalDeviceHandle() const
    {
        return physical_device_;
    }

    VkDevice GetLogicalDeviceHandle() const
    {
        return logical_device_;
    }

    // Infos for supported queue families
    QueueFamilyIndices queue_family_indices_;

    // GPU Info
    std::vector<const char*> device_extensions_;
    std::vector<const char*> device_layers_;
    std::vector<VkQueueFamilyProperties> queue_family_properties_;

private:
    std::vector<const char*> GetRequiredExtensions() const;
    std::vector<const char*> GetRequiredValidationLayers() const;

    VulkanRHI* RHI_ = nullptr;
    int32 device_idx_ = -1;
    bool is_discrete_ = false;

    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physical_device_properties_{};  // basic details, e.g. name, type and supported Vulkan version
    VkPhysicalDeviceFeatures physical_device_features_{};   // supported optional features, e.g. texture compression,
                                                            // 64 bit floats and multi viewport rendering (useful for VR)
    std::vector<const char*> supported_extensions_;
    std::vector<const char*> supported_validation_layers_;

    VkDevice logical_device_ = VK_NULL_HANDLE;

    VulkanQueue* GfxQueue = nullptr;
    VulkanQueue* ComputeQueue = nullptr;
    VulkanQueue* TransferQueue = nullptr;
    VulkanQueue* PresentQueue = nullptr;
};
