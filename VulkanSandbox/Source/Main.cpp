#define GLFW_INCLUDE_VULKAN // GLFW will load vulkan.h
#include <GLFW/glfw3.h>

static std::vector<char> ReadFile(const std::string& filename)
{
    // ate: Start reading at the end of the file -> we can use the read position to determine the file size and allocate a buffer
    // binary : Read the file as binary file (avoid text transformations)
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file!");
    }

    size_t file_size = (size_t)file.tellg();
    std::vector<char> buffer(file_size);

    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();

    return buffer;
}

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
    std::optional<uint32_t> present_family; // Could be the case that the graphics queue family does not support presenting to a surface...

    bool HasFoundQueueFamily()
    {
        return graphics_family.has_value() && present_family.has_value();
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;  // min/max number of images in swap chain, min/max width and height of images
    std::vector<VkSurfaceFormatKHR> surface_formats;    // pixel format, color space
    std::vector<VkPresentModeKHR> present_modes;    // conditions for "swapping" images to the screen
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
        // The instance is the connection between the application and the Vulkan library. We also tell the driver some more information,
        // e.g. what validation layers or extensions we need.
        CreateVulkanInstance();

        // Register our debug callback for validation layers.
        SetupDebugManager();    

        // A surface represents an abstract type to present rendered images to. The surface in our program will be backed by the window that we've already opened with GLFW.
        // We have to create a surface before we select the physical device to ensure that the device meets our requirements.
        CreateSurface(); 

        // Get handle to the physical GPU which meets our requirements.
        SelectPhysicalDevice();

        // Set up a logical device to interface with the physical device.
        // Here we specify which features are required, check which queue families are available and retrieve corresponding queue handles.
        CreateLogicalDevice();

        // Set up infrastructure that will own the frame buffers we render to before transferring them to the screen.
        // Essentially this is a queue of images waiting to be shown on the display. 
        CreateSwapChain();

        // We have to manually retrieve the handles to the images in the swap chain.
        CreateImageViews();

        // Tell Vulkan about the framebuffer attachments that will be used while rendering
        // e.g. how many color and depth buffers there will be, how many samples to use for each of them,
        // how their contents should be handled throughout the rendering, operations,...
        CreateRenderPass();

        // Specify every single thing of the render pipeline stages...
        CreateGraphicsPipeline();

        // The attachments specified during render pass creation are bound by wrapping them into a VkFramebuffer object
        // A framebuffer object references all of the VkImageView objects that represent the attachments.
        // However, the image that we have to use for the attachment depends on which image the swap chain returns when we retrieve one for presentation.
        // That means that we have to create a framebuffer for all of the images in the swap chain and use the one that corresponds to the retrieved image at drawing time.
        CreateFramebuffers();

        // Drawing operations and memory transfers are stored in command buffers. These are retrieved from command pools.
        // We can fill these buffers in multiple threads and then execute them all at once on the main thread.
        CreateCommandPool();

        // Create command buffers for each image in the swap chain.
        CreateCommandBuffers();

        CreateSyncObjects();
    }

    void MainLoop()
    {
        while (!glfwWindowShouldClose(window_))
        {
            glfwPollEvents();
            DrawFrame();
        }

        // operations in drawFrame are asynchronous -> When we exit the loop there may still be some ongoing operations and we shouldn't destroy the resources until we are done using those.
        // => Wait for the logical device to finish operations before exiting mainLoop and destroying the window.
        vkDeviceWaitIdle(logical_device_);
    }

    void Cleanup()
    {
        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(logical_device_, render_finished_semaphores_[i], nullptr);
            vkDestroySemaphore(logical_device_, image_available_semaphores_[i], nullptr);
            vkDestroyFence(logical_device_, inflight_frame_fences_[i], nullptr);
        }

        vkDestroyCommandPool(logical_device_, command_pool_, nullptr);  // Also destroys any command buffers we retrieved from the pool

        for (auto framebuffer : swap_chain_framebuffers_)
        {
            vkDestroyFramebuffer(logical_device_, framebuffer, nullptr);
        }

        vkDestroyPipeline(logical_device_, graphics_pipeline_, nullptr);
        vkDestroyPipelineLayout(logical_device_, pipeline_layout_, nullptr);

        vkDestroyRenderPass(logical_device_, render_pass_, nullptr);

        for (auto image_view : swap_chain_image_views_)
        {
            vkDestroyImageView(logical_device_, image_view, nullptr);
        }

        vkDestroySwapchainKHR(logical_device_, swap_chain_, nullptr);
        vkDestroyDevice(logical_device_, nullptr);

        if(enable_validation_layers_)
        {
            DestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
        }

        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);

        // Clean up glfw
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
        if(CheckInstanceExtensionSupport(extensions))
        {
            create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            create_info.ppEnabledExtensionNames = extensions.data();
        }
        else
        {
            throw std::runtime_error("Required extension not supported!");
        }

        if(vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan instance!");
        }
    }

    std::vector<const char*> GetRequiredExtensions()
    {
        // glfw extensions already include the platform specific extensions which are required
        // e.g. VK_KHR_win32_surface
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

    bool CheckInstanceExtensionSupport(const std::vector<const char*>& required_extensions)
    {
        uint32_t supported_extensions_count = 0;

        // We first have to query the number of supported extensions so we can allocate the required memory
        vkEnumerateInstanceExtensionProperties(nullptr, &supported_extensions_count, nullptr);
        std::vector<VkExtensionProperties> supported_extension_properties(supported_extensions_count);

        // And then we can copy the data to our buffer
        vkEnumerateInstanceExtensionProperties(nullptr, &supported_extensions_count, supported_extension_properties.data());

#ifndef NDEBUG
        std::cout << "Available Vulkan extensions:\n";
        for (const auto& extension_properties : supported_extension_properties)
        {
            std::cout << '\t' << extension_properties.extensionName << '\n';
        }
#endif

        // Check if every required extension is actually available
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

    void SetupDebugManager()
    {
        // Tell Vulkan about our debug callback function in case we use a validation layer.
        if (enable_validation_layers_)
        {
            VkDebugUtilsMessengerCreateInfoEXT create_info{};
            PopulateDebugMessengerCreateInfo(create_info);

            if (CreateDebugUtilsMessengerEXT(instance_, &create_info, nullptr, &debug_messenger_) != VK_SUCCESS)
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

    void CreateSurface()
    {
        // glfw offers a handy abstraction for surface creation.
        // It automatically fills a VkWin32SurfaceCreateInfoKHR struct with the platform specific window and process handles
        // and then calls the platform specific function to create the surface, e.g. vkCreateWin32SurfaceKHR
        if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create window surface!");
        }
    }

    void SelectPhysicalDevice()
    {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);

        if (device_count == 0)
        {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> found_devices(device_count);
        vkEnumeratePhysicalDevices(instance_, &device_count, found_devices.data());

        for (const auto& device : found_devices)
        {
            if (CheckDeviceRequirements(device))
            {
                physical_device_ = device;
                break;
            }
        }

        if (physical_device_ == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Failed to find a GPU that meets requirements!");
        }
    }

    bool CheckDeviceRequirements(VkPhysicalDevice device)
    {
        // We can query basic details, e.g. name, type and supported Vulkan version
        //VkPhysicalDeviceProperties device_properties;
        //vkGetPhysicalDeviceProperties(device, &device_properties);

        // We can also query optional features, e.g. texture compression, 64 bit floats and multi viewport rendering (useful for VR) 
        //VkPhysicalDeviceFeatures device_features;
        //vkGetPhysicalDeviceFeatures(device, &device_features);

        // Additionally, we could check here for more stuff, like the support of geometry shaders, device memory, queue families,...
        // Also, in case of multiple GPUs we could give each physical device a rating and pick the one that fits our needs best, e.g. integrated GPU vs dedicated GPU

        bool are_requirements_met = false;

        QueueFamilyIndices indices = FindQueueFamilies(device);

        bool are_extensions_supported = CheckDeviceExtensionSupport(device);

        bool does_swap_chain_meet_reqs = false;
        if (are_extensions_supported)   // Important: Only try to query for swap chain support after verifying that the swap chain extension is available. 
        {
            SwapChainSupportDetails details = QuerySwapChainSupport(device);

            // Swap chain support is sufficient for us if there is at least one supported image format and one supported presentation mode for the window surface
            does_swap_chain_meet_reqs = details.surface_formats.size() > 0 && details.present_modes.size() > 0;
        }

        are_requirements_met = indices.HasFoundQueueFamily() && are_extensions_supported && does_swap_chain_meet_reqs;

        return are_requirements_met;
    }

    bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
    {
        // We can check for extensions that are tied to a specific device.
        // For example, this is necessary since not every GPU necessarily supports VK_KHR_swapchain (think of GPUs designed for servers...)

        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

        std::set<std::string> required_extensions(device_extensions_.begin(), device_extensions_.end());

        for (const auto& extension : available_extensions)
        {
            required_extensions.erase(extension.extensionName);
        }

        return required_extensions.empty();
    }

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        // VkQueueFamilyProperties contains details about the queue family,
        // e.g. the type of operations that are supported and the number of queues that can be created based on that family
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

            // the graphics queue family does not necessarily also support presenting to a surface
            // -> We have to add an additional check and remember the queue family that supports it.
            // This COULD be the same queue family as the graphics family, though.
            // To maximize performance we could even try to find a family that is required to support both graphics and presenting here.
            VkBool32 is_present_supported = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &is_present_supported);

            if (is_present_supported)
            {
                indices.present_family = i;
            }

            if (indices.HasFoundQueueFamily())
            {
                break;
            }

            i++;
        }

        return indices;
    }

    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device)
    {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

        uint32_t surface_format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &surface_format_count, nullptr);
        if (surface_format_count > 0)
        {
            details.surface_formats.resize(surface_format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &surface_format_count, details.surface_formats.data());
        }

        uint32_t present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &present_mode_count, nullptr);
        if (present_mode_count > 0)
        {
            details.present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &present_mode_count, details.present_modes.data());
        }

        return details;
    }

    void CreateLogicalDevice()
    {
        QueueFamilyIndices indices = FindQueueFamilies(physical_device_);

        // We have to create multiple VkDeviceQueueCreateInfo structs to create a queue for all required families.
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        std::set<uint32_t> unique_queue_families = { indices.graphics_family.value(), indices.present_family.value() };

        float queue_priority = 1.0f;    // Queue priorities [0.0f, 1.0f] influence the scheduling of command buffer execution.
                                        // Required even for a single queue!

        for(uint32_t queue_family : unique_queue_families)
        {
            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = indices.graphics_family.value();
            queue_create_info.queueCount = 1;   // We only need one queue, because we can create command buffers on multiple threads and submit them all at once.
            queue_create_info.pQueuePriorities = &queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        // Specify used device features, e.g. geometry shaders
        // We can query them with vkGetPhysicalDeviceFeatures
        VkPhysicalDeviceFeatures device_features{};

        // Create logical device
        VkDeviceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        create_info.pQueueCreateInfos = queue_create_infos.data();
        create_info.pEnabledFeatures = &device_features;

        // specify device specific extensions
        // For example VK_KHR_swapchain allows the presentation of rendered images from the device to the OS.
        // It could be the case that we use a GPU without this feature, for example if we only rely on compute operations.
        create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions_.size());
        create_info.ppEnabledExtensionNames = device_extensions_.data();

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

        if (vkCreateDevice(physical_device_, &create_info, nullptr, &logical_device_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create logical device!");
        }

        vkGetDeviceQueue(logical_device_, indices.graphics_family.value(), 0, &graphics_queue_);
        vkGetDeviceQueue(logical_device_, indices.present_family.value(), 0, &present_queue_);
    }

    void CreateSwapChain()
    {
        SwapChainSupportDetails swap_chain_support = QuerySwapChainSupport(physical_device_);

        // Choose preferred swap chain properties
        VkSurfaceFormatKHR surface_format = ChooseSwapSurfaceFormat(swap_chain_support.surface_formats);
        VkPresentModeKHR present_mode = ChooseSwapPresentMode(swap_chain_support.present_modes);
        VkExtent2D extent = ChooseSwapExtent(swap_chain_support.capabilities);

        // Specify the minimum num images we would like to have in the swap chain
        uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;   // Minimum + 1 is recommended to avoid GPU stalls.

        // Ensure we don't exceed the supported max image count in the swap chain. 
        bool is_max_image_count_specified = swap_chain_support.capabilities.maxImageCount > 0;  //maxImageCount == 0 means that there is no maximum set by the device!
        if (is_max_image_count_specified && image_count > swap_chain_support.capabilities.maxImageCount)
        {
            image_count = swap_chain_support.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = surface_; // Swap chain is tied to this surface
        create_info.minImageCount = image_count;
        create_info.imageFormat = surface_format.format;
        create_info.imageColorSpace = surface_format.colorSpace;
        create_info.imageExtent = extent;
        create_info.imageArrayLayers = 1;   // Amount of layers each image consists of. 1 unless developing a stereoscopic 3D application.
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;   // What kind of operations images in the swap chain are used for. We'll render directly to them -> Color attachment.
                                                                        // We could also first render to a separate image and then do some post-processing operations.
                                                                        // In that case we may use a value like VK_IMAGE_USAGE_TRANSFER_DST_BIT to use a memory operation to transfer the
                                                                        // rendered image to a swap chain image.

        // Save for later use...
        swap_chain_image_format_ = surface_format.format;
        swap_chain_extent_ = extent;

        // Specify how to handle swap chain images that will be used across multiple queue families 
        // E.g. if graphics queue is different from presentation queue, we'll draw onto images in swap chain from the graphics queue 
        // and then submit them to the presentation queue
        QueueFamilyIndices indices = FindQueueFamilies(physical_device_);
        uint32_t queue_family_indices[] = { indices.graphics_family.value(), indices.present_family.value() };

        if (indices.graphics_family != indices.present_family)
        {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;  // Images can be used across multiple queue families without explicit ownership transfers.
                                                                        // I check out ownership concepts at a later time, so for now we rely on concurrent mode.

            // Concurrent mode requires you to specify in advance between which queue families ownership will be shared
            create_info.queueFamilyIndexCount = 2;  // Has to be at least 2 to use concurrent mode!
            create_info.pQueueFamilyIndices = queue_family_indices;
        }
        else
        {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;   // An image is owned by one queue family at a time and ownership must be explicitly transferred 
                                                                        // before using it in another queue family. This option offers the best performance.
            create_info.queueFamilyIndexCount = 0; // Optional
            create_info.pQueueFamilyIndices = nullptr; // Optional
        }

        // We can specify that a certain transform should be applied to images in the swap chain if it is supported
        // e.g. 90 degree clockwise rotation, horizontal flip,...
        create_info.preTransform = swap_chain_support.capabilities.currentTransform;    // Simply set currentTransform to specify that we don't use any preTransform.

        // Specify if the alpha channel should be used for blending with other windows in the window system
        // VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR -> Ignore the alpha channel.
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        create_info.presentMode = present_mode;
        create_info.clipped = VK_TRUE;  // If true, don't care about pixels that are obscured (e.g. by another window in front).
                                        // Clipping increases performance => only deactivate if really needed.

        // With Vulkan it's possible that the swap chain becomes invalid or unoptimized while the application is running (e.g. due to window resize).
        // => We may have to recreate swap chain from scratch. If so we have to provide a handle to the old swap chain here.
        create_info.oldSwapchain = VK_NULL_HANDLE;  // For now assume that we will only ever create one swap chain.

        if (vkCreateSwapchainKHR(logical_device_, &create_info, nullptr, &swap_chain_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create swap chain!");
        }

        // Retrieve image handles of swap chain.
        vkGetSwapchainImagesKHR(logical_device_, swap_chain_, &image_count, nullptr);
        swap_chain_images_.resize(image_count); // We only specified the minimum num of images, so the swap chain could potentially contain more -> We have to resize!
        vkGetSwapchainImagesKHR(logical_device_, swap_chain_, &image_count, swap_chain_images_.data());
    }

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats)
    {
        for (const auto& available_format : available_formats)
        {
            // Prefer SRGB if it is available -> results in more accurate perceived colors and is standard color space for images / textures.
            if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return available_format;
            }
        }

        // If we can't find our preferred surface format, could rank the available format and choose the next best...
        // For now we'll just use the first available format, which should be okay for most cases.
        return available_formats[0];
    }

    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes)
    {
        // VK_PRESENT_MODE_IMMEDIATE_KHR - Submitted images are transferred to screen right away => Possible tearing. Guaranteed to be available.
        // VK_PRESENT_MODE_FIFO_KHR - Swap chain is fifo queue. Images are taken from queue on display refresh. If queue is full the program has to wait -> Similar to vsync!
        // VK_PRESENT_MODE_FIFO_RELAXED_KHR - Similar to VK_PRESENT_MODE_FIFO_KHR, but if swap chain is empty the next rendered image will be shown instantly -> Possible tearing.
        // VK_PRESENT_MODE_MAILBOX_KHR - Similar to VK_PRESENT_MODE_FIFO_KHR, but if queue is full the application just replaces the already queued images. Can be used for triple buffering.

        for (const auto& available_present_mode : available_present_modes)
        {
            if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return available_present_mode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
    {
        // Swap extent is the resolution of the swap chain images in PIXELS! We have to keep that in mind for high DPI screens, e.g. Retina displays.
        
        // Usually Vulkan tells us to match the window resolution and sets the extends by itself.
        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            return capabilities.currentExtent;
        }
        else // But...
        {
            // Some window managers allow extends that differ from window resolution, as indicated by setting the width and height in currentExtent to max of uint32_t.
            // In that case, pick the resolution that best matches the window within the minImageExtent and maxImageExtent bounds.

            // Important: We have to query the frame buffer size from glfw to get the window extents in PIXELS instead of screen coordinates.
            int width, height;
            glfwGetFramebufferSize(window_, &width, &height);   

            VkExtent2D actual_extent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            // Clamp to range [minImageExtent, maxImageExtent]
            actual_extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actual_extent.width));
            actual_extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actual_extent.height));

            return actual_extent;
        }
    }

    void CreateImageViews()
    {
        // To use any VkImage (e.g. those in the swap chain) in the render pipeline we have to create a VkImageView object.
        // An image view describes how to access the image and which part of the image to access,
        // e.g. if it should be treated as a 2D depth texture without any mipmapping levels.

        swap_chain_image_views_.resize(swap_chain_images_.size());

        for (size_t i = 0; i < swap_chain_images_.size(); ++i)
        {
            VkImageViewCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image = swap_chain_images_[i];
            create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;   // Allows us to treat images as 1D textures, 2D textures, 3D textures and cube maps
            create_info.format = swap_chain_image_format_;

            // components allow us to swizzle color channels, e.g. we could map all of the color channels to the red channel for a monochrome texture, or fill a channel with 0s or 1s.
            create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;   // IDENTITY is the default mapping.
            create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            // The subresourceRange field describes what the image's purpose is and which part of the image should be accessed.
            // Our images will be used as color targets without any mipmapping levels or multiple layers.
            // In case of stereographic 3D applications, we could create a swap chain with multiple layers.
            // We could then create multiple image views for each image representing the views for the left and right eyes by accessing different layers.
            create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            create_info.subresourceRange.baseMipLevel = 0;
            create_info.subresourceRange.levelCount = 1;
            create_info.subresourceRange.baseArrayLayer = 0;
            create_info.subresourceRange.layerCount = 1;

            if (vkCreateImageView(logical_device_, &create_info, nullptr, &swap_chain_image_views_[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create image views!");
            }
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

    void CreateRenderPass()
    {
        // Specify how many color and depth buffers there will be, how many samples to use for each of them and
        // how their contents should be handled throughout the rendering operations
        VkAttachmentDescription color_attachment{};
        color_attachment.format = swap_chain_image_format_; // should match the format of the swap chain images
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;   // No multisampling for now -> Only 1 sample.
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  // What to do with the data in the attachment before rendering
                                                                //VK_ATTACHMENT_LOAD_OP_LOAD: Preserve the existing contents of the attachment
                                                                //VK_ATTACHMENT_LOAD_OP_CLEAR : Clear the values to a constant at the start
                                                                //VK_ATTACHMENT_LOAD_OP_DONT_CARE : Existing contents are undefined; we don't care about them
                                                                // => We clear the screen to black before drawing a new frame.

        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;    // What to do with the data in the attachment after rendering
                                                                    // VK_ATTACHMENT_STORE_OP_STORE: Rendered contents will be stored in memory and can be read later
                                                                    // VK_ATTACHMENT_STORE_OP_DONT_CARE : Contents of the framebuffer will be undefined after the rendering operation
                                                                    // => We're interested in seeing the rendered polygons on the screen, so we're going with the store operation here.

        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;   // Our application won't do anything with the stencil buffer
                                                                            // -> the results of loading and storing are irrelevant
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  

        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Specifies which layout the image will have before the render pass begins.
                                                                    // Layout of the pixels in memory can change based on what you're trying to do with an image.
                                                                    // Common layouts:
                                                                    // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: Images used as color attachment
                                                                    // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : Images to be presented in the swap chain
                                                                    // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : Images to be used as destination for a memory copy operation

        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // specifies the layout to automatically transition to when the render pass finishes
                                                                        // We want the image to be ready for presentation using the swap chain after rendering.

        // Subpasses and attachment references
        // A single render pass can consist of multiple subpasses. Subpasses are subsequent rendering operations that depend on the contents of frame buffers in previous passes,
        // e.g. a sequence of post-processing effects that are applied one after another.
        // Grouping these rendering operations into one render pass, Vulkan is able to reorder the operations and conserve memory bandwidth for possibly better performance.
        
        // Every subpass references one or more of the attachments that we've described using the structure in the previous sections
        // The following other types of attachments can be referenced by a subpass:
        // pInputAttachments: Attachments that are read from a shader
        // pResolveAttachments : Attachments used for multisampling color attachments
        // pDepthStencilAttachment : Attachment for depth and stencil data
        // pPreserveAttachments : Attachments that are not used by this subpass, but for which the data must be preserved
        VkAttachmentReference color_attachment_ref{};
        color_attachment_ref.attachment = 0;    // specifies which attachment to reference by its index in the attachment descriptions array of VkRenderPassCreateInfo
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // specifies which layout we would like the attachment to have during a subpass that uses this reference
                                                                                // Vulkan will automatically transition the attachment to this layout when the subpass is started

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;    // We have to be explicit that this is a graphics subpass. Could also be a compute subpass in the future!
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;

        // Subpass dependencies
        // Subpasses in a render pass automatically take care of image layout transitions. 
        // These transitions are controlled by subpass dependencies, which specify memory and execution dependencies between subpasses. 
        // We have only a single subpass right now, but the operations right before and right after this subpass also count as implicit "subpasses".

        // There are two built-in dependencies that take care of the transition at the start of the render pass and at the end of the render pass, 
        // BUT: The start dependency assumes that the transition occurs at the start of the pipeline, but we haven't acquired the image yet at that point!
        // => does not occur at the right time! 
        // Solution 1: Change the waitStages for the imageAvailableSemaphore to VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT to ensure that the render passes don't begin until the image is available
        // Solution 2: Make the render pass wait for the VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT stage <- We do this here.

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;    // Dependency. special value VK_SUBPASS_EXTERNAL refers to the implicit subpass before or after the render pass 
                                                        // depending on whether it is specified in srcSubpass or dstSubpass
        dependency.dstSubpass = 0;  // Dependent subpass (we only have one here). dst Must always be higher than src to prevent cycles in dependency graph!

        // Specify the operations to wait on and the stages in which these operations occur
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;    // We need to wait for the swap chain to finish reading from the image before we can access it
                                                                                    // -> Wait on color attachment output stage itself
        dependency.srcAccessMask = 0;

        // Prevent the transition from happening until it's actually necessary (and allowed): when we want to start writing colors to it.
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;    // The operations that should wait on this are in the color attachment stage  
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;            // and involve the writing of the color attachment

        // Finally create the render pass
        VkRenderPassCreateInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments = &color_attachment;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;

        if (vkCreateRenderPass(logical_device_, &render_pass_info, nullptr, &render_pass_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create render pass!");
        }
    }

    void CreateGraphicsPipeline()
    {
        // Load shader byte code
        auto vs_source = ReadFile("assets/shaders/vert.spv");
        auto fs_source = ReadFile("assets/shaders/frag.spv");

        // Create shader modules
        // Shader modules are just a thin wrapper around the shader bytecode that we've previously loaded from a file and the functions defined in it.
        // The compilation and linking of the SPIR-V bytecode to machine code for execution by the GPU doesn't happen until the graphics pipeline is created.
        // That means that we're allowed to destroy the shader modules again as soon as pipeline creation is finished
        VkShaderModule vert_shader_module = CreateShaderModule(vs_source);
        VkShaderModule frag_shader_module = CreateShaderModule(fs_source);

        // To actually use the shaders we'll need to assign them to a specific pipeline stage
        VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
        vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;  // Indicate the pipeline stage here.
        vert_shader_stage_info.module = vert_shader_module;
        vert_shader_stage_info.pName = "main";  // The entry point of the shader. Allows us to pack multiple shaders into a single shader module.
        vert_shader_stage_info.pSpecializationInfo = nullptr;   // Optional. Allows to specify values for shader constants. -> Allows compiler optimizations like eliminating branches...

        VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
        frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_shader_stage_info.module = frag_shader_module;
        frag_shader_stage_info.pName = "main";
        vert_shader_stage_info.pSpecializationInfo = nullptr;

        VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_stage_info, frag_shader_stage_info };

        // Vertex input: Describe the format of the vertex data that will be passed to the vertex shader
        // Bindings -> spacing between data and whether the data is per-vertex or per-instance
        // Attribute descriptions -> type of the attributes passed to the vertex shader, which binding to load them from and at which offset
        VkPipelineVertexInputStateCreateInfo vertex_input_info{};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount = 0;    // Temporarily set this to 0 (vertices are hardcoded in the shader atm)
        vertex_input_info.pVertexBindingDescriptions = nullptr; // Optional
        vertex_input_info.vertexAttributeDescriptionCount = 0;  // Temporarily set this to 0 (vertices are hardcoded in the shader atm)
        vertex_input_info.pVertexAttributeDescriptions = nullptr;   // Optional

        // Input Assembly: What kind of geometry will be drawn from the vertices (e.g. VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,...)
        // and if primitive restart should be enabled.
        VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
        input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_info.primitiveRestartEnable = VK_FALSE;  // if true, it's possible to break up lines and triangles in _STRIP topology modes by
                                                                // using a special index of 0xFFFF or 0xFFFFFFFF

        // Viewports and scissors
        // Viewport describes the region of the framebuffer that output will be rendered to (almost always (0, 0) to (width, height))
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swap_chain_extent_.width);  // We'll use the swap chain images as frame buffers, so we use their corresponding extent
        viewport.height = static_cast<float>(swap_chain_extent_.height);
        viewport.minDepth = 0.0f;   // must be in range [0.0, 1.0]
        viewport.maxDepth = 1.0f;   // must be in range [0.0, 1.0]

        // Scissor rectangles define in which regions pixels will actually be stored.
        // Any pixels outside the scissor rectangles will be discarded by the rasterizer.
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swap_chain_extent_;

        // Combine both viewport and scissor rect into a viewport state
        VkPipelineViewportStateCreateInfo viewport_state_info{};
        viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_info.viewportCount = 1;  // Some GPUs support multiple
        viewport_state_info.pViewports = &viewport;
        viewport_state_info.scissorCount = 1;   // Some GPUs support multiple
        viewport_state_info.pScissors = &scissor;

        // Rasterizer: takes the geometry that is shaped by the vertices from the vertex shader and turns it into fragments to be colored by the fragment shader.
        // Also performs depth testing, face culling and the scissor test, can be configured to output fragments that fill entire polygons or just the edges (wireframe rendering). 
        VkPipelineRasterizationStateCreateInfo rasterizer_info{};
        rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer_info.depthClampEnable = VK_FALSE;        // If true, fragments beyond near/far planes are clamped to them as opposed to being discarded.
        rasterizer_info.rasterizerDiscardEnable = VK_FALSE; // If true, geometry never passes through the rasterizer stage, basically disabling any output to the framebuffer.
        rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL; // determines how fragments are generated for geometry
                                                            // VK_POLYGON_MODE_FILL: Fill polygon area with fragments
                                                            // VK_POLYGON_MODE_LINE: Draw polygon edges as lines (wireframe) -> requires enabling as GPU feature
                                                            // VK_POLYGON_MODE_POINT: Draw polygon vertices as points -> requires enabling as GPU feature
        rasterizer_info.lineWidth = 1.0f;   // Thickness of lines in terms of number of fragments. Max depends on hardware. Value > 1.0f require enabling of "wideLines" GPU feature.
        rasterizer_info.cullMode = VK_CULL_MODE_BACK_BIT;    // Regular culling logic: Front face, back face, both, or disabled.
        rasterizer_info.frontFace = VK_FRONT_FACE_CLOCKWISE; // Specifies the vertex order for faces to be considered front-facing
        rasterizer_info.depthBiasEnable = VK_FALSE;         // If true, the rasterizer will add a bias to the depth values (sometimes used for shadow mapping).
        rasterizer_info.depthBiasConstantFactor = 0.0f;     // Optional
        rasterizer_info.depthBiasClamp = 0.0f;              // Optional
        rasterizer_info.depthBiasSlopeFactor = 0.0f;        // Optional

        // Multisampling - AA technique combining the fragment shader results of multiple polygons that rasterize to the same pixel. 
        // Because it doesn't need to run the fragment shader multiple times if only one polygon maps to a pixel, it is significantly less expensive than
        // simply rendering to a higher resolution and then downscaling.
        // Enabling it requires enabling a GPU feature.
        VkPipelineMultisampleStateCreateInfo multisampling_info{};
        multisampling_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling_info.sampleShadingEnable = VK_FALSE;
        multisampling_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling_info.minSampleShading = 1.0f; // Optional
        multisampling_info.pSampleMask = nullptr; // Optional
        multisampling_info.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling_info.alphaToOneEnable = VK_FALSE; // Optional

        // Depth and stencil testing
        // If you are using a depth and/or stencil buffer, then you also need to configure the depth and stencil tests using VkPipelineDepthStencilStateCreateInfo
        // We don't have one right now, so we can simply pass a nullptr instead of a pointer to such a struct.

        // Color blending -> Commonly used for alpha blending.
        // After a fragment shader has returned a color, it needs to be combined with the color that is already in the framebuffer. This transformation is known as color blending.
        // -> Either mix the old and new value to produce a final color or combine the old and new value using a bitwise operation.
        VkPipelineColorBlendAttachmentState color_blend_attachment_info{};  // Configuration per attached framebuffer
        color_blend_attachment_info.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment_info.blendEnable = VK_TRUE;  // if VK_FALSE, then the new color from the fragment shader is passed through unmodified
                                                            // else the two mixing operations are performed to compute a new color
                                                            // The resulting color is AND'd with the colorWriteMask to determine which channels are actually passed through.
        color_blend_attachment_info.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_info.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment_info.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_info.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_info.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment_info.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo color_blending_info{};    // Global color blending settings
        color_blending_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending_info.logicOpEnable = VK_FALSE;
        color_blending_info.logicOp = VK_LOGIC_OP_COPY; // Optional
        color_blending_info.attachmentCount = 1;
        color_blending_info.pAttachments = &color_blend_attachment_info;
        color_blending_info.blendConstants[0] = 0.0f; // Optional
        color_blending_info.blendConstants[1] = 0.0f; // Optional
        color_blending_info.blendConstants[2] = 0.0f; // Optional
        color_blending_info.blendConstants[3] = 0.0f; // Optional

        // Dynamic state - stuff that can actually be changed without recreating the pipeline
        // e.g. size of the viewport, line width and blend constants.
        // Specifying this will cause the configuration of these values to be ignored and we will be required to specify the data at drawing time.
        // Can be nullptr if we don't use dynamic states.
         
        //VkDynamicState dynamic_states[] = {
        //    VK_DYNAMIC_STATE_VIEWPORT,
        //    VK_DYNAMIC_STATE_LINE_WIDTH
        //};

        //VkPipelineDynamicStateCreateInfo dynamic_state_info{};
        //dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        //dynamic_state_info.dynamicStateCount = 2;
        //dynamic_state_info.pDynamicStates = dynamic_states;

        // Pipeline layout - Describes the usage of uniforms.
        // Uniform values are globals similar to dynamic state variables that can be changed at drawing time to alter the behavior of your shaders without having to recreate them.
        // They are commonly used to pass the transformation matrix to the vertex shader, or to create texture samplers in the fragment shader.
        // Even if we don't use any we have to create an empty pipeline layout.
        // Also since we create it, we also have to clean it up later on!
        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 0; // Optional
        pipeline_layout_info.pSetLayouts = nullptr; // Optional
        pipeline_layout_info.pushConstantRangeCount = 0; // Optional, push constants are another way of passing dynamic values to shaders 
        pipeline_layout_info.pPushConstantRanges = nullptr; // Optional

        if (vkCreatePipelineLayout(logical_device_, &pipeline_layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipeline_create_info{};
        pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_create_info.stageCount = 2;
        pipeline_create_info.pStages = shader_stages;
        pipeline_create_info.pVertexInputState = &vertex_input_info;
        pipeline_create_info.pInputAssemblyState = &input_assembly_info;
        pipeline_create_info.pViewportState = &viewport_state_info;
        pipeline_create_info.pRasterizationState = &rasterizer_info;
        pipeline_create_info.pMultisampleState = &multisampling_info;
        pipeline_create_info.pDepthStencilState = nullptr; // Optional
        pipeline_create_info.pColorBlendState = &color_blending_info;
        pipeline_create_info.pDynamicState = nullptr; // Optional
        pipeline_create_info.layout = pipeline_layout_;
        pipeline_create_info.renderPass = render_pass_;
        pipeline_create_info.subpass = 0;   // index of the sub pass where this graphics pipeline will be used
        pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;   // Optional. Vulkan allows creation of a new graphics pipeline by deriving from an existing pipeline
                                                                    // Deriving is less expensive to set up when pipelines have lots of functionality in common and
                                                                    // switching between pipelines from the same parent can be done quicker.
        pipeline_create_info.basePipelineIndex = -1; // Optional

        // Time to create the graphics pipeline!
        uint32_t create_info_count = 1; // We could create multiple render pipelines at once.
        VkPipelineCache pipeline_cache = VK_NULL_HANDLE;    // can be used to store and reuse data relevant to pipeline creation across multiple calls to vkCreateGraphicsPipelines
                                                            // and even across program executions if the cache is stored to a file. 
        if (vkCreateGraphicsPipelines(logical_device_, pipeline_cache, create_info_count, &pipeline_create_info, nullptr, &graphics_pipeline_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        // Finally clean up the shader modules
        vkDestroyShaderModule(logical_device_, frag_shader_module, nullptr);
        vkDestroyShaderModule(logical_device_, vert_shader_module, nullptr);
    }

    VkShaderModule CreateShaderModule(const std::vector<char>& code)
    {
        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = code.size();
        create_info.pCode = reinterpret_cast<const uint32_t*>(code.data()); // Have to reinterpret cast here because we got char* but uint32_t* is expected.

        VkShaderModule shader_module;
        if (vkCreateShaderModule(logical_device_, &create_info, nullptr, &shader_module) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shader module!");
        }

        return shader_module;
    }

    void CreateFramebuffers()
    {
        // Create frame buffer for each image view in our swap chain
        swap_chain_framebuffers_.resize(swap_chain_image_views_.size());
        for (size_t i=0; i < swap_chain_image_views_.size(); i++)
        {
            VkImageView attachments[] = {
                swap_chain_image_views_[i]
            };

            VkFramebufferCreateInfo frambuffer_info{};
            frambuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            frambuffer_info.renderPass = render_pass_;  // Framebuffer needs to be compatible with this render pass -> Use same number and types of attachments
            frambuffer_info.attachmentCount = 1;        // Right now we only have one color attachment
            frambuffer_info.pAttachments = attachments; // Specify the VkImageView objects that should be bound to the respective attachment descriptions in the
                                                        // render pass pAttachment array.
            frambuffer_info.width = swap_chain_extent_.width;
            frambuffer_info.height = swap_chain_extent_.height;
            frambuffer_info.layers = 1; // swap chain images are single images -> 1 layer.

            if (vkCreateFramebuffer(logical_device_, &frambuffer_info, nullptr, &swap_chain_framebuffers_[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }

    }

    void CreateCommandPool()
    {

        // Command buffers are executed by submitting them on one of the device queues, like the graphics and presentation queues we retrieved. 
        // Each command pool can only allocate command buffers that are submitted on a single type of queue. 
        QueueFamilyIndices queue_family_indices = FindQueueFamilies(physical_device_);
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = queue_family_indices.graphics_family.value();   // We only use drawing commands, so we stick to the graphics queue family.
        pool_info.flags = 0;    // Optional.
                                // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often (may change memory allocation behavior)
                                // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be rerecorded individually.
                                // Without this flag they all have to be reset together.
                                // For now we will only fill the command buffer once at the beginning of the program, so we don't use on any of the flags.
                           
        if (vkCreateCommandPool(logical_device_, &pool_info, nullptr, &command_pool_) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void CreateCommandBuffers()
    {
        // Because one of the drawing commands involves binding the right VkFramebuffer, we have to record a command buffer for every image in the swap chain.
        command_buffers_.resize(swap_chain_framebuffers_.size());

        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = command_pool_;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // Specifies if the allocated command buffers are primary or secondary command buffers
                                                            // VK_COMMAND_BUFFER_LEVEL_PRIMARY -> Can be submitted to a queue for execution, but cannot be called from other command buffers
                                                            // VK_COMMAND_BUFFER_LEVEL_SECONDARY -> Cannot be submitted directly, but can be called from primary command buffers.
        alloc_info.commandBufferCount = static_cast<uint32_t>(command_buffers_.size());

        if (vkAllocateCommandBuffers(logical_device_, &alloc_info, command_buffers_.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate command buffers!");
        }

        // For now also record the command buffer since we want to show a static triangle
        for (size_t i = 0; i < command_buffers_.size(); ++i)
        {
            VkCommandBufferBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = 0;   // Optional.
                                    // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will be rerecorded right after executing it once.
                                    // VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: This is a secondary command buffer that will be entirely within a single render pass.
                                    // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: The command buffer can be resubmitted while it is also already pending execution.
            begin_info.pInheritanceInfo = nullptr;  // Optional. Specifies which state to inherit from the calling primary command buffers.
                                                    // Only relevant for secondary command buffers.

            if (vkBeginCommandBuffer(command_buffers_[i], &begin_info) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo render_pass_info{};
            render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            render_pass_info.renderPass = render_pass_;
            render_pass_info.framebuffer = swap_chain_framebuffers_[i];
            render_pass_info.renderArea.offset = { 0, 0 };
            render_pass_info.renderArea.extent = swap_chain_extent_;    // Pixels outside this region will have undefined values.
                                                                        // It should match the size of the attachments for best performance.
        
            // define the clear values to use for VK_ATTACHMENT_LOAD_OP_CLEAR
            VkClearValue clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
            render_pass_info.clearValueCount = 1;
            render_pass_info.pClearValues = &clear_color;

            vkCmdBeginRenderPass(command_buffers_[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(command_buffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);

            // We've now told Vulkan which operations to execute in the graphics pipeline and which attachment to use in the fragment shader,
            // so all that remains is telling it to draw the triangle...
            uint32_t vertex_count = 3;      // We don't have the vertex buffer yet, but technically we still want to draw 3 vertices...
            uint32_t instance_count = 1;    // We only render one instance
            uint32_t first_vertex = 0;      // Offset into the vertex buffer
            uint32_t first_instance = 0;    // offset for instanced rendering
            vkCmdDraw(command_buffers_[i], vertex_count, instance_count, first_vertex , first_instance);

            vkCmdEndRenderPass(command_buffers_[i]);

            if (vkEndCommandBuffer(command_buffers_[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to record command buffer!");
            }
        }
    }

    void DrawFrame()
    {
        // Wait for requested frame to be finished
        vkWaitForFences(logical_device_, 1, &inflight_frame_fences_[current_frame_], VK_TRUE /*wait for all fences until return*/, UINT64_MAX /*disable time out*/);

        // Drawing a frame involves these operations, which will be executed asynchronously with a single function call:
        //  * Acquire an image from the swap chain
        //  * Execute the command buffer with that image as attachment in the framebuffer
        //  * Return the image to the swap chain for presentation
        // Since this is async, the execution order is undefined. Yet the operations depend on each other => We have to synchronize.
        
        // Fences and semaphores are both objects that can be used for coordinating operations,
        // e.g. by having one operation signal and another operation wait for a fence or semaphore to go from the unsignaled to signaled state.
        // Fences can be accessed from the application (vkWaitForFences), semaphores can't.
        // Fences are designed to synchronize the application itself with rendering operations
        // Semaphores are used to sync operations within or across command queues.
        // => We want to synchronize the queue operations of draw commands and presentation, which makes semaphores the best fit.
    
        uint32_t image_index;   // refers to the VkImage idx in our swap_chain_images_ array
        vkAcquireNextImageKHR(logical_device_, swap_chain_, UINT64_MAX /*disable time out*/, image_available_semaphores_[current_frame_], VK_NULL_HANDLE, &image_index);

        // If MAX_FRAMES_IN_FLIGHT is higher than the number of swap chain images or vkAcquireNextImageKHR returns images out-of-order 
        // it's possible that we may start rendering to a swap chain image that is already in flight.
        // To avoid this, we need to track for each swap chain image if a frame in flight is currently using it.
        if (inflight_images_[image_index] != VK_NULL_HANDLE)
        {
            vkWaitForFences(logical_device_, 1, &inflight_images_[image_index], VK_TRUE, UINT64_MAX);
        }
        // Mark the image as now being in use by this frame
        inflight_images_[image_index] = inflight_frame_fences_[current_frame_];

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore wait_semaphores[] = { image_available_semaphores_[current_frame_] };  // which semaphores to wait on before execution begins 
        VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; // in which stages of the pipeline to wait
                                                                                                // We want to wait with writing colors to the image until it's available,
                                                                                                // so we're specifying the stage of the graphics pipeline that writes to the color attachment
                                                                                                // => Theoretically the implementation can already start executing our vertex shader etc
                                                                                                // while the image is not yet available
                                                                                                // Each entry in the array corresponds to the semaphore with the same index in pWaitSemaphores
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;

        // Submit the command buffer that binds the swap chain image we just acquired as color attachment
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffers_[image_index];

        // Specify which semaphores to signal once the command buffer(s) have finished execution
        VkSemaphore signal_semaphores[] = { render_finished_semaphores_[current_frame_] };
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        vkResetFences(logical_device_, 1, &inflight_frame_fences_[current_frame_]);  // restore the fence to the unsignaled state 

        if (vkQueueSubmit(graphics_queue_, 1, &submit_info, inflight_frame_fences_[current_frame_]) != VK_SUCCESS)  // Takes an array of VkSubmitInfo structs as argument for efficiency 
                                                                                                                    // when the workload is much larger
                                                                                                                    // Last parameter is optional fence that will be
                                                                                                                    // signaled when command buffers finish execution
        {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        // Finally submit result back to the swap chain to have it eventually show up on the screen 
        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        // Which semaphores to wait on before presentation can happen
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;

        // Specify the swap chains to present images to and the index of the image for each swap chain (will almost always be a single one)
        VkSwapchainKHR swap_chains[] = { swap_chain_ };
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swap_chains;
        present_info.pImageIndices = &image_index;
        present_info.pResults = nullptr;    // Optional. Allows to specify an array of VkResult values to check for every individual swap chain if presentation was successful
                                            // Not necessary if you're only using a single swap chain, because you can simply use the return value of the present function.

        // Submits the request to present an image to the swap chain
        vkQueuePresentKHR(present_queue_, &present_info);

        // Advance the frame index
        current_frame_ = (++current_frame_) % MAX_FRAMES_IN_FLIGHT;
    }

    void CreateSyncObjects()
    {
        image_available_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
        render_finished_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
        inflight_frame_fences_.resize(MAX_FRAMES_IN_FLIGHT);
        inflight_images_.resize(swap_chain_images_.size(), VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // By default we create fences in unsignaled state
                                                         // -> We'd wait indefinitely because we never submitted the fence before

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(logical_device_, &semaphore_info, nullptr, &image_available_semaphores_[i]) != VK_SUCCESS ||
                vkCreateSemaphore(logical_device_, &semaphore_info, nullptr, &render_finished_semaphores_[i]) != VK_SUCCESS ||
                vkCreateFence(logical_device_, &fence_info, nullptr, &inflight_frame_fences_[i]) != VK_SUCCESS)
            {

                throw std::runtime_error("Failed to create semaphores!");
            }
        }
    }

    static const int MAX_FRAMES_IN_FLIGHT = 2;  // How many frames should be processed concurrently

    GLFWwindow* window_ = nullptr;
    const uint32_t SCREEN_WIDTH = 800;
    const uint32_t SCREEN_HEIGHT = 600;

    VkInstance instance_ = VK_NULL_HANDLE;  // The connection between the application and the Vulkan library
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;  // We do not have to clean this up manually
    VkDevice logical_device_ = VK_NULL_HANDLE;

    const std::vector<const char*> valiation_layers_ = { "VK_LAYER_KHRONOS_validation" };
#ifdef NDEBUG
    const bool enable_validation_layers_ = false;
#else
    const bool enable_validation_layers_ = true;
#endif

    const std::vector<const char*> device_extensions_ = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };    // Availability of a present queue implicitly ensures that swapchains are supported
                                                                                                // but being explicit is good practice. Also we have to explicitly enable the extension
                                                                                                // anyway...

    VkQueue graphics_queue_ = VK_NULL_HANDLE;   // We do not have to clean this up manually, clean up of logical device takes care of this.
    VkQueue present_queue_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR swap_chain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swap_chain_images_; // image handles will be automatically cleaned up by destruction of swap chain.
    VkFormat swap_chain_image_format_;
    VkExtent2D swap_chain_extent_;
    std::vector<VkImageView> swap_chain_image_views_;   // Will be explicitly created by us -> We have to clean them up!
    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> swap_chain_framebuffers_;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers_;
    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> inflight_frame_fences_;
    std::vector<VkFence> inflight_images_;
    uint32_t current_frame_ = 0;
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
