#include "VulkanInstance.h"

#include "VulkanMacros.h"

namespace 
{
    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& in_debug_messenger_create_info)
    {
        in_debug_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        in_debug_messenger_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        in_debug_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        in_debug_messenger_create_info.pfnUserCallback = VulkanDebugUtils::DebugCallback;
    }
}

void VulkanInstance::Init()
{
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    // This is optional, but may provide crucial information to the graphics driver to optimize the application.
    // E.g. we could provide information about our well-known engine (Unity, Unreal,...) which the driver knows about.
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Vulkan Sandbox";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;
    create_info.pApplicationInfo = &app_info;

    QuerySupportedInstanceExtensions();
    QuerySupportedInstanceValidationLayers();

    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info;
    SetupEnabledValidationLayers(create_info, debug_messenger_create_info);
    SetupEnabledExtensions(create_info);

    if(vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS)
    {
        LOG_ERROR("Failed to create Vulkan instance.");
        exit(EXIT_FAILURE);
    }

    SetupDebugMessenger();
}

void VulkanInstance::Shutdown()
{
    CHECK(instance_ != VK_NULL_HANDLE);
    vkDestroyInstance(instance_, nullptr);
}

void VulkanInstance::QuerySupportedInstanceExtensions()
{
    uint32 extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    supported_extensions_.resize(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, supported_extensions_.data());
}

void VulkanInstance::QuerySupportedInstanceValidationLayers()
{
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    supported_validation_layers_.resize(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, supported_validation_layers_.data());
}

void VulkanInstance::SetupEnabledValidationLayers(VkInstanceCreateInfo& in_create_info, VkDebugUtilsMessengerCreateInfoEXT& in_debug_messenger_create_info)
{
#ifdef _RENDER_DEBUG
    required_validation_layers_.push_back("VK_LAYER_KHRONOS_validation");
    in_create_info.enabledLayerCount = static_cast<uint32_t>(required_validation_layers_.size());
    in_create_info.ppEnabledLayerNames = required_validation_layers_.data();

    // We can't use the regular DebugMessenger, because it needs an initialized instance before it can be created.
    // -> Create an additional debug messenger which will automatically be used during vkCreateInstance and vkDestroyInstance and cleaned up after that.
    // See https://github.com/KhronosGroup/Vulkan-Docs/blob/master/appendices/VK_EXT_debug_utils.txt#L120
    PopulateDebugMessengerCreateInfo(in_debug_messenger_create_info);
    in_debug_messenger_create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&in_debug_messenger_create_info;
#else
    in_create_info.enabledLayerCount = 0;
    in_create_info.pNext = nullptr;
#endif
}

void VulkanInstance::SetupDebugMessenger()
{
#ifdef _RENDER_DEBUG
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info{};
    PopulateDebugMessengerCreateInfo(debug_messenger_create_info);

    // The debug messenger is an extension => We have to look up the address of the function ourselves
    PFN_vkCreateDebugUtilsMessengerEXT CreateDebugMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT");
    CHECK(CreateDebugMessenger != nullptr);
    VERIFY_VK_RESULT(CreateDebugMessenger(instance_, &debug_messenger_create_info, nullptr, &debug_messenger_));
#endif // _RENDER_DEBUG
}

void VulkanInstance::SetupEnabledExtensions(VkInstanceCreateInfo& in_create_info)
{
#ifdef _RENDER_DEBUG
    required_extensions_.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    if (AreExtensionsSupport())
    {
        LOG("Enabled extenstions: ");
        for(const char* ext : required_extensions_)
        {
            LOG("- {}", ext);
        }

        in_create_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions_.size());
        in_create_info.ppEnabledExtensionNames = required_extensions_.data();
    }
    else
    {
        LOG_ERROR("Failed to create Vulkan instance: Some required extensions are not supported.");
        exit(EXIT_FAILURE);
    }
}

bool VulkanInstance::AreExtensionsSupport()
{
    std::vector<const char*> missing_extensions;

    for (const char* ext : required_extensions_)
    {
        bool found_extension = false;
        for (const auto& supported_ext_property : supported_extensions_)
        {
            found_extension = strcmp(ext, supported_ext_property.extensionName) == 0;
            if (found_extension)
            {
                break;
            }
        }

        if(found_extension == false)
        {
            missing_extensions.push_back(ext);
        }
    }

    bool all_exts_supported = missing_extensions.size() <= 0;
    if(all_exts_supported == false)
    {
        LOG_ERROR("The following extensions are required but not supported: ");
        for(const auto& ext : missing_extensions)
        {
            LOG_ERROR("- {}", ext);
        }
    }

    return all_exts_supported;
}
