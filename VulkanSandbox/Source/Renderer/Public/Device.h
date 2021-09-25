#pragma once
#include "vulkan/vulkan_core.h"

struct QueueFamilyIndices
{
    // Every value could be potentially valid, so we have to rely on optional.
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family; // Could be the case that the graphics queue family does not support presenting to a surface...
    std::optional<uint32_t> transfer_family;
    std::optional<uint32_t> compute_family;
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
struct Device 
{
    explicit Device(VkPhysicalDevice physical_device);
    ~Device();

    /**
    * Create logical device based on the physical device.
    *
    * @param device_creation_props DeviceCreationProperties struct defining the settings to create the device
    * @return VkResult of the vk API call to create the device
    */
    VkResult CreateLogicalDevice(const DeviceCreationProperties& device_creation_props);

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

    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice logical_device_ = VK_NULL_HANDLE;

    // basic details, e.g. name, type and supported Vulkan version
    VkPhysicalDeviceProperties physical_device_properties {};

    // supported optional features, e.g. texture compression, 64 bit floats and multi viewport rendering (useful for VR)
    VkPhysicalDeviceFeatures physical_device_features {};

    // Infos for supported queue families
    QueueFamilyIndices queue_family_indices_;

    // List of supported extensions
    std::vector<std::string> supported_extensions_;
};

