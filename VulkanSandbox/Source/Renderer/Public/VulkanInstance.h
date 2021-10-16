#pragma once
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_win32.h>

#include "VulkanDebugUtils.h"

/**
*   The instance is the connection between the application and the Vulkan library.
*   We also tell the driver some more information, e.g. what validation layers or extensions we need.
*/
class VulkanInstance
{
public:
    void Init();
    void Shutdown();

    VkInstance GetHandle() const
    {
        return instance_;
    }

private:
    /**
    *    Fills supported_instance_extensions_ with data queried from the driver.
    */
    void QuerySupportedInstanceExtensions();

    /**
    *    Fills supported_validation_layers_ with data queried from the driver.
    */
    void QuerySupportedInstanceValidationLayers();

    /**
    *    Tell the driver which global validation layers to enable and sets up the debug callback.
    *    Automatically adds VK_LAYER_KHRONOS_validation to required layers if in render debug configuration.
    *    
    *    @param in_create_info The VkInstanceCreateInfo struct to fill.
    *    @param in_debug_messenger_create_info  The VkDebugUtilsMessengerCreateInfoEXT struct to fill.
    */
    void SetupEnabledValidationLayers(VkInstanceCreateInfo& in_create_info, VkDebugUtilsMessengerCreateInfoEXT& in_debug_messenger_create_info);

    /**
    *    If DEBUG_RENDER is defined, create a debug messenger, so any interesting messages are forwarded to our logging utility.
    */
    void SetupDebugMessenger();

    /**
    *    If DEBUG_RENDER is defined, destroy the debug messenger.
    */
    void DestroyDebugMessenger();

    /**
    *   Tell the driver which global extensions to enable.
    *   (Global extensions are extensions which are applied to the entire program instead of a specific device)
    *   Automatically adds VK_EXT_DEBUG_UTILS_EXTENSION_NAME to required extensions if in render debug configuration.
    *   
    *   @param in_create_info The VkInstanceCreateInfo struct to fill.
    */
    void SetupEnabledExtensions(VkInstanceCreateInfo& in_create_info);

    /**
    *   Check if every required extension is actually available
    *   
    *   @return True if all required_extensions_ are supported.
    */
    bool AreExtensionsSupport();

    VkInstance instance_ = VK_NULL_HANDLE;
    std::vector<VkExtensionProperties> supported_extensions_;
    std::vector<VkLayerProperties> supported_validation_layers_;

    std::vector<const char*> required_extensions_ = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME }; // TODO: this should depend on the platform
    std::vector<const char*> required_validation_layers_;

    VkDebugUtilsMessengerEXT debug_messenger_;
};
