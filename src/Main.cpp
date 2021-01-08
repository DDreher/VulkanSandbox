#define GLFW_INCLUDE_VULKAN // GLFW will load vulkan.h
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>

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

    VkInstance vulkan_instance_ = nullptr;
    VkDebugUtilsMessengerEXT vulkan_debug_messenger_ = nullptr;

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