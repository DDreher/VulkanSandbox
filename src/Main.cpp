#define GLFW_INCLUDE_VULKAN // GLFW will load vulkan.h
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>

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
        vkDestroyInstance(vulkan_instance_, nullptr);

        glfwDestroyWindow(window_);
        glfwTerminate();
    }

    void CreateVulkanInstance()
    {
        // This is optional, but may provide crucial information to the graphics driver to optimize the application.
        // E.g. we could provide information about our well-known engine (Unity, Unreal,...) which the driver knows about.
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Hello Triangle";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "No Engine";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_0;

        // Check which Vulkan extensions are supported
        uint32_t extension_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> extensions(extension_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());

        // Ensure that all the extensions required by glfw are supported
        std::cout << "Available Vulkan extensions:\n";
        for (const auto& extension : extensions)
        {
            std::cout << '\t' << extension.extensionName << '\n';
        }
        uint32_t glfw_extension_count = 0;
        const char** glfw_extensions;
        glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

        const char* string = *glfw_extensions;

        for(size_t i=0; i<glfw_extension_count; ++i)
        {
            const char* glfw_ext = glfw_extensions[i];

            bool found_extension = false;
            for(const auto& ext : extensions)
            {
                found_extension = strcmp(glfw_ext, ext.extensionName) == 0;
                if(found_extension)
                {
                    break;
                }
            }

            if (found_extension == false)
            {
                throw std::runtime_error("Required extension not supported!");
            }
        }

        VkInstanceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;
        create_info.enabledExtensionCount = glfw_extension_count; // Tell the driver which global extensions are used.
        create_info.ppEnabledExtensionNames = glfw_extensions;    // Global extensions are extensions which are applied to the entire program instead of a specific device.
        create_info.enabledLayerCount = 0; // Tell the driver which global validation layers to enable

        if(vkCreateInstance(&create_info, nullptr, &vulkan_instance_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan instance!");
        }
    }

    GLFWwindow* window_ = nullptr;
    const uint32_t SCREEN_WIDTH = 800;
    const uint32_t SCREEN_HEIGHT = 600;

    VkInstance vulkan_instance_ = nullptr;
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