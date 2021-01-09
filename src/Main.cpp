#define GLFW_INCLUDE_VULKAN // GLFW will load vulkan.h
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    // This is a proxy function encapsulating the creation process of a Vulkan debug utils messenger used to see validation layer outputs.
    // In order to create the messenger we have to call vkCreateDebugUtilsMessengerEXT, which is an extension function 
    // => We have to look up the address of this function ourselves.
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        // Function couldn't be loaded
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger, const VkAllocationCallbacks* allocator)
{
    // The VkDebugUtilsMessengerEXT object also needs to be cleaned up with a call to vkDestroyDebugUtilsMessengerEXT. 
    // Similarly to vkCreateDebugUtilsMessengerEXT the function needs to be explicitly loaded.
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debug_messenger, allocator);
    }
}

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphics_family; // Every value could be potentially valid, so we have to rely on optional.

    bool HasFoundQueueFamily()
    {
        return graphics_family.has_value();
    }
};

class HelloTriangleApplication
{
public:
    void Run()
    {
        InitWindow();
        InitVulkan();
        MainLoop();
        Cleanup();
    }

private:
    void InitWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);   // Prevent creation of OpenGL context
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);     // Disable window resize
        window_ = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Vulkan Sandbox", nullptr, nullptr);
    }

    void InitVulkan()
    {
        CreateVulkanInstance();
        SetupDebugManager();
        SelectPhysicalDevice();
        CreateLogicalDevice();
    }

    void MainLoop()
    {
        while (!glfwWindowShouldClose(window_))
        {
            glfwPollEvents();
        }
    }

    void Cleanup()
    {
        vkDestroyDevice(vulkan_logical_device_, nullptr);

        if(enable_validation_layers_)
        {
            DestroyDebugUtilsMessengerEXT(vulkan_instance_, vulkan_debug_messenger_, nullptr);
        }

        vkDestroyInstance(vulkan_instance_, nullptr);

        glfwDestroyWindow(window_);
        glfwTerminate();
    }

    void CreateVulkanInstance()
    {
        if(enable_validation_layers_ && CheckValidationLayerSupport() == false)
        {
            throw std::runtime_error("Required extension not supported!");
        }

        // This is optional, but may provide crucial information to the graphics driver to optimize the application.
        // E.g. we could provide information about our well-known engine (Unity, Unreal,...) which the driver knows about.
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Hello Triangle";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "No Engine";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;

        // Tell the driver which global validation layers to enable
        VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info;
        if (enable_validation_layers_)
        {
            create_info.enabledLayerCount = static_cast<uint32_t>(valiation_layers_.size());
            create_info.ppEnabledLayerNames = valiation_layers_.data();

            // Create an additional debug messenger which will automatically be used during vkCreateInstance and vkDestroyInstance and cleaned up after that.
            PopulateDebugMessengerCreateInfo(debug_messenger_create_info);
            create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debug_messenger_create_info;
        }
        else
        {
            create_info.enabledLayerCount = 0;
            create_info.pNext = nullptr;
        }

        // Tell the driver which global extensions are used.
        // Global extensions are extensions which are applied to the entire program instead of a specific device.
        std::vector<const char*> extensions = GetRequiredExtensions();
        if(CheckExtensionSupport(extensions))
        {
            create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            create_info.ppEnabledExtensionNames = extensions.data();
        }
        else
        {
            throw std::runtime_error("Required extension not supported!");
        }

        if(vkCreateInstance(&create_info, nullptr, &vulkan_instance_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan instance!");
        }
    }

    std::vector<const char*> GetRequiredExtensions()
    {
        uint32_t glfw_extension_count;
        const char** glfw_extensions;
        glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

        if (enable_validation_layers_)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool CheckExtensionSupport(const std::vector<const char*>& required_extensions)
    {
        uint32_t supported_extensions_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &supported_extensions_count, nullptr);
        std::vector<VkExtensionProperties> supported_extension_properties(supported_extensions_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &supported_extensions_count, supported_extension_properties.data());

#ifndef NDEBUG
        std::cout << "Available Vulkan extensions:\n";
        for (const auto& extension_properties : supported_extension_properties)
        {
            std::cout << '\t' << extension_properties.extensionName << '\n';
        }
#endif

        for(const char* required_ext : required_extensions)
        {
            bool found_extension = false;
            for (const auto& supported_ext_property : supported_extension_properties)
            {
                found_extension = strcmp(required_ext, supported_ext_property.extensionName) == 0;
                if (found_extension)
                {
                    break;
                }
            }

            if (found_extension == false)
            {
                return false;
            }
        }

        return true;
    }

    void SelectPhysicalDevice()
    {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(vulkan_instance_, &device_count, nullptr);

        if (device_count == 0)
        {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> found_devices(device_count);
        vkEnumeratePhysicalDevices(vulkan_instance_, &device_count, found_devices.data());

        for (const auto& device : found_devices)
        {
            if (CheckDeviceRequirements(device))
            {
                vulkan_physical_device_ = device;
                break;
            }
        }

        if (vulkan_physical_device_ == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Failed to find a GPU that meets requirements!");
        }
    }

    bool CheckDeviceRequirements(VkPhysicalDevice device)
    {
        // Query basic details, e.g. name, type and supported Vulkan version
        //VkPhysicalDeviceProperties device_properties;
        //vkGetPhysicalDeviceProperties(device, &device_properties);

        // Query optional features, e.g. texture compression, 64 bit floats and multi viewport rendering (useful for VR) 
        //VkPhysicalDeviceFeatures device_features;
        //vkGetPhysicalDeviceFeatures(device, &device_features);

        // We could check here for more stuff, like the support of geometry shaders, device memory, queue families,...
        // Also, in case of multiple GPUs we could give each physical device a rating and pick the one that fits our needs best, e.g. integrated GPU vs dedicated GPU

        QueueFamilyIndices indices = FindQueueFamilies(device);
        return indices.HasFoundQueueFamily();
    }

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        // VkQueueFamilyProperties contains details about the queue family, including the type of operations that are supported and the number of queues that can be created based on that family
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        // We need to find at least one queue family that supports VK_QUEUE_GRAPHICS_BIT.
        int i = 0;
        for (const auto& queue_family : queue_families)
        {
            if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphics_family = i;
            }

            if (indices.HasFoundQueueFamily())
            {
                break;
            }

            i++;
        }

        return indices;
    }

    void CreateLogicalDevice()
    {
        QueueFamilyIndices indices = FindQueueFamilies(vulkan_physical_device_);

        // Specify queues to be created
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = indices.graphics_family.value();
        queue_create_info.queueCount = 1;   // We only need one queue, because we can create command buffers on multiple thread and submit them all at once.
        float queue_priority = 1.0f;
        queue_create_info.pQueuePriorities = &queue_priority; // Queue priorities [0.0f, 1.0f] influence the scheduling of command buffer execution. Required even for 1 queue!

        // Specify used device features
        // (We can query them with vkGetPhysicalDeviceFeatures, e.g. geometry shaders)
        VkPhysicalDeviceFeatures device_features{};

        // Create logical device
        VkDeviceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.pQueueCreateInfos = &queue_create_info;
        create_info.queueCreateInfoCount = 1;
        create_info.pEnabledFeatures = &device_features;

        // specify device specific extensions
        // For example VK_KHR_swapchain allows the presentation of rendered images from the device to the OS.
        // It could be the case that we use a GPU without this feature, for example if we only rely on compute operations.
        create_info.enabledExtensionCount = 0;  // For now we don't need any device specific extensions.

        // Specify device specific validation layers
        // Previous implementations of Vulkan made a distinction between instance and device specific validation layers, but this is no longer the case
        // It is still good practice to set the values to be compatible with older implementations.
        if (enable_validation_layers_)
        {
            create_info.enabledLayerCount = static_cast<uint32_t>(valiation_layers_.size());
            create_info.ppEnabledLayerNames = valiation_layers_.data();
        }
        else
        {
            create_info.enabledLayerCount = 0;
        }

        if (vkCreateDevice(vulkan_physical_device_, &create_info, nullptr, &vulkan_logical_device_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create logical device!");
        }
    }

    bool CheckValidationLayerSupport()
    {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

        for (const char* layer_name : valiation_layers_)
        {
            bool layer_found = false;

            for (const auto& layer_properties : available_layers)
            {
                if (strcmp(layer_name, layer_properties.layerName) == 0)
                {
                    layer_found = true;
                    break;
                }
            }

            if (layer_found == false)
            {
                return false;
            }
        }

        return true;
    }

    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info)
    {
        create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        create_info.pfnUserCallback = DebugCallback;
    }

    void SetupDebugManager()
    {
        // Tell Vulkan about our debug callback function in case we use a validation layer.
        if (enable_validation_layers_)
        {
            VkDebugUtilsMessengerCreateInfoEXT create_info{};
            PopulateDebugMessengerCreateInfo(create_info);

            if (CreateDebugUtilsMessengerEXT(vulkan_instance_, &create_info, nullptr, &vulkan_debug_messenger_) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to set up debug messenger!");
            }
        }
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
    {
        std::cerr << "Validation layer: " << callback_data->pMessage << std::endl;

        // Return value indicates if the Vulkan call that triggered the validation layer message should be aborted with VK_ERROR_VALIDATION_FAILED_EXT.
        // Usually this is only used to test validation layers. -> We should most likely always return VK_FALSE here.
        return VK_FALSE; 
    }

    GLFWwindow* window_ = nullptr;
    const uint32_t SCREEN_WIDTH = 800;
    const uint32_t SCREEN_HEIGHT = 600;

    VkInstance vulkan_instance_ = VK_NULL_HANDLE;  // The connection between the application and the Vulkan library
    VkDebugUtilsMessengerEXT vulkan_debug_messenger_ = VK_NULL_HANDLE;
    VkPhysicalDevice vulkan_physical_device_ = VK_NULL_HANDLE;  // We do not have to clean this up manually
    VkDevice vulkan_logical_device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;   // We do not have to clean this up manually, clean up of logical device takes care of this.

    const std::vector<const char*> valiation_layers_ = { "VK_LAYER_KHRONOS_validation" };
#ifdef NDEBUG
    const bool enable_validation_layers_ = false;
#else
    const bool enable_validation_layers_ = true;
#endif

};

int main()
{
    HelloTriangleApplication app;

    try
    {
        app.Run();
    }
    catch (const std::exception & e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
