#define GLFW_INCLUDE_VULKAN // GLFW will load vulkan.h
#define GLM_FORCE_RADIANS   // Ensure that matrix functions use radians as units
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // GLM perspective projection matrix will use depth range of -1.0 to 1.0 by default. We need range of 0.0 to 1.0 for Vulkan.
#define GLM_ENABLE_EXPERIMENTAL // Needed so we can use the hash functions of GLM types
#include <chrono>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // matrix functions like glm::lookAt etc.
#include <glm/gtx/hash.hpp>
#include <stb/stb_image.h>
#include <tiny_obj_loader.h>

struct Vertex 
{
    glm::vec3 pos_;
    glm::vec3 color_;
    glm::vec2 tex_coords_;

    static VkVertexInputBindingDescription GetBindingDescription()
    {
        // Describes how to pass data to the vertex shader.
        // Specifies number of bytes between data entries and the input rate, i.e. whether to move to next data entry
        // after each vertex or after each instance
        VkVertexInputBindingDescription binding_description{};
        binding_description.binding = 0;    // Specifies index of the binding in an array of bindings. 
                                            // Our data is in one array, so we have only one binding.
        binding_description.stride = sizeof(Vertex);    // Number of bytes from one entry to the next
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;    // VK_VERTEX_INPUT_RATE_VERTEX: Move to the next data entry after each vertex
                                                                        // VK_VERTEX_INPUT_RATE_INSTANCE: Move to the next data entry after each instance
                                                                        // In this case we stick to per-vertex data.
        return binding_description;
    }

    static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
    {
        // Describes how to extract a vertex attribute from a chunk of vertex data coming from a binding description
        // -> We have three attributes (pos, color, UVs), so we need two attribute descriptions.
        // -> We need add UVs as vertex input attribute description so we can pass them on to the fragment shader as interpolated value!
        std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions{};

        // Pos attribute
        attribute_descriptions[0].binding = 0;  // Which binding does the per-vertex data come from?
        attribute_descriptions[0].location = 0; // References the location of the attribute in the vertex shader
        attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; // Data type of the attribute. Implicitely defines the byte size of the attribute data
                                                                    // -> For some reason we have to use the color format enums here... Weird af
                                                                    // float: VK_FORMAT_R32_SFLOAT      
                                                                    // vec2 : VK_FORMAT_R32G32_SFLOAT
                                                                    // vec3 : VK_FORMAT_R32G32B32_SFLOAT
                                                                    // vec4 : VK_FORMAT_R32G32B32A32_SFLOAT
                                                                    // -> In this case it's the position, which has three 32bit float components (i.e. r,g,b channel)
                                                                    // If we specify less components than are actually required, then the BGA channels default to (0.0f, 0.0f, 1.0f).
                                                                    // -> SFLOAT means signed float. There's also UINT, SINT. Should match the type of the shader input
                                                                    
        attribute_descriptions[0].offset = offsetof(Vertex, pos_);  // Specifies the number of bytes since the start of the per-vertex data to read from
                                                                    // Binding is loading one vertex at a time and pos is at an offset of 0 bytes from the beginning of the struct.
                                                                    // -> We can easily calculate this with the offsetof macro though!

        // Color attribute
        attribute_descriptions[1].binding = 0;
        attribute_descriptions[1].location = 1;
        attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions[1].offset = offsetof(Vertex, color_);

        // Tex coords attribute
        attribute_descriptions[2].binding = 0;
        attribute_descriptions[2].location = 2;
        attribute_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attribute_descriptions[2].offset = offsetof(Vertex, tex_coords_);

        return attribute_descriptions;
    }

    // Need this so we can use hash maps
    bool operator==(const Vertex& other) const
    {
        return pos_ == other.pos_ && color_ == other.color_ && tex_coords_ == other.tex_coords_;
    }
};

// hash function for our Vertex struct
namespace std
{
    template<> struct hash<Vertex>
    {
        size_t operator()(Vertex const& vertex) const
        {
            // Create hash according to http://en.cppreference.com/w/cpp/utility/hash
            return ((hash<glm::vec3>()(vertex.pos_) ^
                (hash<glm::vec3>()(vertex.color_) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.tex_coords_) << 1);
        }
    };
}

struct UniformBufferObject
{
    // Vulkan expects data in structure to be aligned in memory in a specific way: 
    // Scalars have to be aligned by N(= 4 bytes given 32 bit floats).
    // A vec2 must be aligned by 2N(= 8 bytes)
    // A vec3 or vec4 must be aligned by 4N(= 16 bytes)
    // A nested structure must be aligned by the base alignment of its members rounded up to a multiple of 16.
    // A mat4 matrix must have the same alignment as a vec4.
    // See: https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout

    // Best practice: Always be explicit about alignment!
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

static std::vector<char> ReadFile(const std::string& filename)
{
    // ate: Start reading at the end of the file -> we can use the read position to determine the file size and allocate a buffer
    // binary : Read the file as binary file (avoid text transformations)
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file: " + filename);
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

        window_ = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Vulkan Sandbox", nullptr, nullptr);
        glfwSetWindowUserPointer(window_, this);    // Save pointer to app, so we can access it on frame buffer resize
        glfwSetFramebufferSizeCallback(window_, FramebufferResizeCallback);
    }

    static void FramebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        // Have to use a static function here, because GLFW doesn't pass the pointer to our application.
        // However, glfw allows us to store our pointer with glfwSetWindowUserPointer. :) Yay.
        
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->was_frame_buffer_resized_ = true;
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

        // Specify the types of resources that are going to be accessed by the pipeline
        CreateDescriptorSetLayout();

        // Specify every single thing of the render pipeline stages...
        CreateGraphicsPipeline();

        // Drawing operations and memory transfers are stored in command buffers. These are retrieved from command pools.
        // We can fill these buffers in multiple threads and then execute them all at once on the main thread.
        CreateCommandPool();

        // Init resources for MSAA
        CreateColorResources();

        // Init resources for depth buffering
        CreateDepthResources();

        // The attachments specified during render pass creation are bound by wrapping them into a VkFramebuffer object
        // A framebuffer object references all of the VkImageView objects that represent the attachments.
        // However, the image that we have to use for the attachment depends on which image the swap chain returns when we retrieve one for presentation.
        // That means that we have to create a framebuffer for all of the images in the swap chain and use the one that corresponds to the retrieved image at drawing time.
        CreateFramebuffers();

        // Load an image and upload it into a Vulkan image object
        CreateTextureImage();
        CreateTextureImageView();
        CreateTextureSampler();

        // Populate vertices and indices
        LoadModel();

        // Create and allocate buffers for our model we want to render
        // We can further optimize this by storing both vertex and index buffer in a single vkBuffer to make it more cache friendly
        // See: https://developer.nvidia.com/vulkan-memory-management
        // We could even reuse the same chunk of memory for multiple resources if they are used during different render operations. (keyword: "aliasing")
        CreateVertexBuffer();
        CreateIndexBuffer();
        CreateUniformBuffers();

        CreateDescriptorPool();
        CreateDescriptorSets();

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
        CleanUpSwapChain();

        vkDestroySampler(logical_device_, texture_sampler_, nullptr);
        vkDestroyImageView(logical_device_, texture_image_view_, nullptr);

        vkDestroyImage(logical_device_, texture_image_, nullptr);
        vkFreeMemory(logical_device_, texture_image_memory_, nullptr);

        vkDestroyDescriptorSetLayout(logical_device_, descriptor_set_layout_, nullptr);

        // Destroy buffers and corresponding memory
        vkDestroyBuffer(logical_device_, index_buffer_, nullptr);
        vkFreeMemory(logical_device_, index_buffer_memory_, nullptr);
        vkDestroyBuffer(logical_device_, vertex_buffer_, nullptr);
        vkFreeMemory(logical_device_, vertex_buffer_memory_, nullptr);

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(logical_device_, render_finished_semaphores_[i], nullptr);
            vkDestroySemaphore(logical_device_, image_available_semaphores_[i], nullptr);
            vkDestroyFence(logical_device_, inflight_frame_fences_[i], nullptr);
        }

        vkDestroyCommandPool(logical_device_, command_pool_, nullptr);  // Also destroys any command buffers we retrieved from the pool

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
                num_msaa_samples_ = GetMaxNumSamples();
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

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
        bool are_features_supported = supportedFeatures.samplerAnisotropy;

        are_requirements_met = indices.HasFoundQueueFamily() && are_extensions_supported && does_swap_chain_meet_reqs && are_features_supported;

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

        // Specify used device features, e.g. geometry shaders or anisotropic filtering
        // We can query them with vkGetPhysicalDeviceFeatures
        VkPhysicalDeviceFeatures device_features{};
        device_features.samplerAnisotropy = VK_TRUE;

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

    void CleanUpSwapChain()
    {
        // multisampled color buffer (MSAA)
        vkDestroyImageView(logical_device_, color_image_view_, nullptr);
        vkDestroyImage(logical_device_, color_image_, nullptr);
        vkFreeMemory(logical_device_, color_image_memory_, nullptr);

        // depth buffer
        vkDestroyImageView(logical_device_, depth_image_view_, nullptr);
        vkDestroyImage(logical_device_, depth_image_, nullptr);
        vkFreeMemory(logical_device_, depth_image_memory_, nullptr);

        for (auto framebuffer : swap_chain_framebuffers_)
        {
            vkDestroyFramebuffer(logical_device_, framebuffer, nullptr);
        }

        // We don't have to recreate the whole command pool.
        vkFreeCommandBuffers(logical_device_, command_pool_, static_cast<uint32_t>(command_buffers_.size()), command_buffers_.data());

        vkDestroyPipeline(logical_device_, graphics_pipeline_, nullptr);
        vkDestroyPipelineLayout(logical_device_, pipeline_layout_, nullptr);

        vkDestroyRenderPass(logical_device_, render_pass_, nullptr);

        for (auto image_view : swap_chain_image_views_)
        {
            vkDestroyImageView(logical_device_, image_view, nullptr);
        }

        vkDestroySwapchainKHR(logical_device_, swap_chain_, nullptr);

        // Clean up uniform buffer here, as it depends on the number of images in the swap chain.
        for (size_t i = 0; i < swap_chain_images_.size(); i++)
        {
            vkDestroyBuffer(logical_device_, uniform_buffers_[i], nullptr);
            vkFreeMemory(logical_device_, uniform_buffers_memory_[i], nullptr);
        }

        // The descriptor pool also depends on the number of swap chain images
        vkDestroyDescriptorPool(logical_device_, descriptor_pool_, nullptr);
    }

    // Recreate SwapChain and all things depending on it.
    void RecreateSwapChain()
    {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window_, &width, &height);

        // In case we minimize the frame buffer will have size 0.
        // -> We pause the application until it has a frame buffer with a valid size again.
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(window_, &width, &height);
            glfwWaitEvents();
        }

        // Wait until resources aren't used anymore
        vkDeviceWaitIdle(logical_device_);  

        // ^^^ This kinda sucks, because we have to stop rendering in order to recreate the swap chain.
        // We could pass the old swap chain object to the vkSwapchainCreateInfoKHR struct and then destroy the old swap chain
        // as soon as we're finished with it.

        // Clean up old objects
        CleanUpSwapChain();

        // Then recreate swap chain itself, and subsequently everything that depends on it
        CreateSwapChain();  
        CreateImageViews(); // -> Are based directly on the swap chain images
        CreateRenderPass(); // -> Depends on the format of the swap chain (format probably won't change, but it doesn't hurt to handle this case)
        CreateGraphicsPipeline();   // -> Viewport and scissor rectangle size is specified here.
                                    // We could skip this by using a dynamic state for the viewport / scissor rects
         
        CreateColorResources();
        CreateDepthResources();

        // These directly depend on the swap chain images
        CreateFramebuffers();
        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();
        CreateCommandBuffers();
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

    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t num_mips)
    {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = format;
        view_info.subresourceRange.aspectMask = aspect_flags;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = num_mips;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VkImageView image_view;
        if (vkCreateImageView(logical_device_, &view_info, nullptr, &image_view) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create image view!");
        }

        return image_view;
    }

    void CreateImageViews()
    {
        // To use any VkImage (e.g. those in the swap chain) in the render pipeline we have to create a VkImageView object.
        // An image view describes how to access the image and which part of the image to access,
        // e.g. if it should be treated as a 2D depth texture without any mipmapping levels.

        swap_chain_image_views_.resize(swap_chain_images_.size());

        for (size_t i = 0; i < swap_chain_images_.size(); ++i)
        {
            swap_chain_image_views_[i] = CreateImageView(swap_chain_images_[i], swap_chain_image_format_, VK_IMAGE_ASPECT_COLOR_BIT, 1);
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
        color_attachment.samples = num_msaa_samples_;   // No multisampling for now -> Only 1 sample.
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

        color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // specifies the layout to automatically transition to when the render pass finishes
                                                                                 // multisampled images cannot be presented directly. We first need to resolve them to a regular image.
                                                                                 // (does not apply to depth buffer, since it won't be presented)

        // MSAA: Have to add a new attachment so we can resolve the multisampled color image to a regular image attachment with only a single sample
        VkAttachmentDescription color_attachment_resolve{};
        color_attachment_resolve.format = swap_chain_image_format_;
        color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depth_attachment{};
        depth_attachment.format = FindDepthFormat();
        depth_attachment.samples = num_msaa_samples_;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;    // depth data will not be used after drawing has finished (may allow hardware optimizations)
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;     // we don't care about the previous depth contents
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

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

        VkAttachmentReference depth_attachment_ref{};
        depth_attachment_ref.attachment = 1;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment_resolve_ref{};
        color_attachment_resolve_ref.attachment = 2;
        color_attachment_resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;    // We have to be explicit that this is a graphics subpass. Could also be a compute subpass in the future!
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;
        subpass.pDepthStencilAttachment = &depth_attachment_ref;    //  a subpass can only use a single depth (+stencil) attachment
        subpass.pResolveAttachments = &color_attachment_resolve_ref;    // This is enough to let the render pass define a multisample resolve operation
                                                                        // which will let us render the image to screen

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

        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;    
        // ^^^ We need to wait for the swap chain to finish reading from the image before we can access it
        // The depth image is first accessed in the early fragment test pipeline 
        dependency.srcAccessMask = 0;

        // Prevent the transition from happening until it's actually necessary (and allowed): when we want to start writing colors to it.

        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        // ^^^ The operations that should wait on this are in the color attachment stage / early fragment tests stage
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        // ^^^ and involve the writing of the color attachment
        // and clearing the depth buffer

        // Finally create the render pass
        VkRenderPassCreateInfo render_pass_info{};
        std::array<VkAttachmentDescription, 3> attachments = { color_attachment, depth_attachment, color_attachment_resolve };
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        render_pass_info.pAttachments = attachments.data();
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;

        if (vkCreateRenderPass(logical_device_, &render_pass_info, nullptr, &render_pass_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create render pass!");
        }
    }

    void CreateDescriptorSetLayout()
    {
        // The descriptor layout specifies the types of resources that are going to be accessed by the pipeline,
        // just like a render pass specifies the types of attachments that will be accessed. 
         
        VkDescriptorSetLayoutBinding ubo_layout_binding{};
        ubo_layout_binding.binding = 0; // Binding index used in the shader
        ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo_layout_binding.descriptorCount = 1; // We could provide an array of UBOs by increasing the count, e.g. if we have multiple UBOs for bone transformations
        ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // In which shader is this gonna be used? Can be combination of multiple bits or even VK_SHADER_STAGE_ALL_GRAPHICS
        ubo_layout_binding.pImmutableSamplers = nullptr; // Optional. Only relevant for image sampling related descriptors.

        VkDescriptorSetLayoutBinding sampler_layout_binding{};
        sampler_layout_binding.binding = 1;
        sampler_layout_binding.descriptorCount = 1;
        sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;   // We could theoretically also sample a texture in the vertex shader, e.g. to deform the vertices!
        sampler_layout_binding.pImmutableSamplers = nullptr;

        // Create the desriptor set layout using our bindings
        std::array<VkDescriptorSetLayoutBinding, 2> bindings = { ubo_layout_binding, sampler_layout_binding };
        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size()); // Accepts array of bindings -> We have to specify the count
        layout_info.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(logical_device_, &layout_info, nullptr, &descriptor_set_layout_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create descriptor set layout!");
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

        auto binding_description = Vertex::GetBindingDescription();
        auto attribute_descriptions = Vertex::GetAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertex_input_info{};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount = 1;
        vertex_input_info.pVertexBindingDescriptions = &binding_description;
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
        vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

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
        rasterizer_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Specifies the vertex order for faces to be considered front-facing
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
        multisampling_info.rasterizationSamples = num_msaa_samples_;
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

        VkPipelineDepthStencilStateCreateInfo depth_stencil_info{};
        depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_info.depthTestEnable = VK_TRUE; // Compare depth of new fragments to depth buffer to see if they should be discarded
        depth_stencil_info.depthWriteEnable = VK_TRUE;    // New depth of fragments which pass the depth test should be written to the depth buffer
        depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS;   // Lower depth -> closer. Fragments with depth less than depth buffer will pass the test.
        depth_stencil_info.depthBoundsTestEnable = VK_FALSE;  // This would allow us to only keep fragments which fall into a specified depth range
        depth_stencil_info.minDepthBounds = 0.0f; // Optional
        depth_stencil_info.maxDepthBounds = 1.0f; // Optional
        depth_stencil_info.stencilTestEnable = VK_FALSE;  // Won't be using the stencil buffer right now
        depth_stencil_info.front = {}; // Optional
        depth_stencil_info.back = {}; // Optional

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
        // Uniform values are globals similar to dynamic state variables that can be changed at drawing time to alter the behavior of your shaders 
        // without having to recreate them.
        // They are commonly used to pass the transformation matrix to the vertex shader, or to create texture samplers in the fragment shader.
        // Even if we don't use any we have to create an empty pipeline layout.
        // Also since we create it, we also have to clean it up later on!
        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1; // Optional
        pipeline_layout_info.pSetLayouts = &descriptor_set_layout_; // Optional
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
        pipeline_create_info.pDepthStencilState = &depth_stencil_info; // Have to add this if we use a depth attachment
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
            // This has to be in the correct order, as specified in the render pass!
            std::array<VkImageView, 3> attachments = 
            {
                color_image_view_,
                depth_image_view_,  // depth buffer can be used by all of the swap chain images, because only a single subpass is running at the same time
                swap_chain_image_views_[i] // Color attachment differs for every swap chain image
            };

            VkFramebufferCreateInfo frambuffer_info{};
            frambuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            frambuffer_info.renderPass = render_pass_;  // Framebuffer needs to be compatible with this render pass -> Use same number and types of attachments
            frambuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
            frambuffer_info.pAttachments = attachments.data(); // Specify the VkImageView objects that should be bound to the respective attachment descriptions
                                                               // in the render pass pAttachment array.
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

    VkCommandBuffer BeginSingleTimeCommands()
    {
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = command_pool_;
        alloc_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer;
        vkAllocateCommandBuffers(logical_device_, &alloc_info, &command_buffer);

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(command_buffer, &begin_info);

        return command_buffer;
    }

    void EndSingleTimeCommands(VkCommandBuffer command_buffer)
    {
        vkEndCommandBuffer(command_buffer);

        // Submit the command buffer to complete the operations
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;

        vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);

        // Execute transfer immediately. We could use a fence to wait for this to be executed
        // or we simply wait for the transfer queue to be idle.
        // -> A fence would allow us to schedule multiple transfers at the same time instead of doing one transfer at a time.
        // -> There's more room for performance optimizations
        vkQueueWaitIdle(graphics_queue_);

        // TODO: 
        // Combine these operations in a single command buffer and execute them asynchronously for higher throughput, especially the transitions and copy in the createTextureImage function.
        // Try to experiment with this by creating a setupCommandBuffer that the helper functions record commands into, and add a flushSetupCommands to execute the commands that have 
        // been recorded so far. It's best to do this after the texture mapping works to check if the texture resources are still set up correctly.

        // Once the transfer is done we can clean up.
        vkFreeCommandBuffers(logical_device_, command_pool_, 1, &command_buffer);
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
            // IMPORTANT: order of clear_values should be identical to the order of attachments
            std::array<VkClearValue, 2> clear_values{};
            clear_values[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values[1].depthStencil = { 1.0f, 0 }; // 0.0 is at the near view plane, 1.0 lies at the far view plane.
                                                        // Initial value should be furthest possible depth, i.e. 1.0

            render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());;
            render_pass_info.pClearValues = clear_values.data();

            vkCmdBeginRenderPass(command_buffers_[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(command_buffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);

            // We've now told Vulkan which operations to execute in the graphics pipeline and which attachment to use in the fragment shader,
            // so all that remains is binding the vertex buffer and drawing the triangle
            VkBuffer vertex_buffers[] = { vertex_buffer_ };
            VkDeviceSize offsets[] = { 0 };

            // Bind vertex buffer to bindings
            vkCmdBindVertexBuffers(command_buffers_[i], 0 /*offset*/, 1 /*num bindings*/,
                vertex_buffers, offsets /*byte offsets to start reading the data from*/); 
            vkCmdBindIndexBuffer(command_buffers_[i], index_buffer_, 0 /*offset*/, VK_INDEX_TYPE_UINT32);   // We can only bind one index buffer!
                                                                                                            // Can't use different indices for each vertex attribute (e.g. for normals)
                                                                                                            // Also: If we have uint32 indices, we have to adjust the type!

            // Bind descriptor set to the descriptors in the shader
            vkCmdBindDescriptorSets(command_buffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, // <- have to specify if we bind to graphics or compute pipeline
                pipeline_layout_, 0, 1, &descriptor_sets_[i], 0, nullptr);
            vkCmdDrawIndexed(command_buffers_[i], static_cast<uint32_t>(indices_.size()), 1, 0, 0, 0);

            //vkCmdDraw(command_buffers_[i], static_cast<uint32_t>(vertices.size()), 1, 0, 0);  // <-- Draws without index buffer
            vkCmdDrawIndexed(command_buffers_[i], static_cast<uint32_t>(indices_.size()), 1, 0, 0, 0); // <- Draws with index buffer

            vkCmdEndRenderPass(command_buffers_[i]);

            if (vkEndCommandBuffer(command_buffers_[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to record command buffer!");
            }
        }
    }

    uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties)
    {
        // GPU may offer different types of memory which differ in terms of allowed operations or performance.
        // This function helps to find the available memory which suits our needs best.

        // First query info about available memory types of the physical device
        // VkPhysicalDeviceMemoryProperties::memoryHeaps -> distinct memory resources (e.g. dedicated VRAM or swap space in RAM when VRAM is depleted)
        // VkPhysicalDeviceMemoryProperties::memoryTypes -> types which exist inside the memoryHeaps.
        VkPhysicalDeviceMemoryProperties mem_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_properties);

        // Then find a memory type that is suitable for the buffer itself
        for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
        {
            // type_filer specifies the bit field of memory types that are suitable
            // -> We simply check if the bit is set for the memory types we want to accept
            bool is_type_accepted = type_filter & (1 << i);
            if(is_type_accepted)
            {
                // We also have to check for the properties of the memory!
                // For example, we may want to be able to write to a vertex buffer from the CPU, so it has to support
                // being mapped to the host.
                // We may have multiple requested properties, so we have to use a bitwise AND operation to check if ALL properties are
                // supported.
                bool are_required_properties_supported = (mem_properties.memoryTypes[i].propertyFlags & properties) == properties;
                if (are_required_properties_supported)
                {
                    return i;
                }
            }
        }

        // Welp, we're screwed.
        throw std::runtime_error("failed to find suitable memory type!");
    }

    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& out_buffer, VkDeviceMemory& out_buffer_memory)
    {
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = usage; // Specify how the buffer is used. Can be multiple with bitwise or.
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;    // Buffers can be owned by specific queue families or shared between multiple queue families.
                                                                // This buffer will only be used by the graphics queue, so we use exclusive access.
        buffer_info.flags = 0;  // Used to configure sparse buffer memory (not relevant for us right now)

        if (vkCreateBuffer(logical_device_, &buffer_info, nullptr, &out_buffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create vertex buffer");
        }

        // Buffer was created, but no memory has been allocated yet.
        // We have to do this ourselves!

        // First query memory requirements.
        VkMemoryRequirements mem_requirements;
        vkGetBufferMemoryRequirements(logical_device_, out_buffer, &mem_requirements);

        // Then allocate the memory
        // NOTE: In a real application, we shouldn't allocate memory for every single resource we create. (inefficient / max num of simultaneous mem allocations is limited)
        // Instead we should allocate a large chunk of memory and then split it up with the offset parameters by using a custom allocator.
        // See https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator for examples
        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_requirements.size;
        alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, properties);

        if (vkAllocateMemory(logical_device_, &alloc_info, nullptr, &out_buffer_memory) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate vertex buffer memory!");
        }

        // Finally associate the allocated memory with the vertex buffer
        vkBindBufferMemory(logical_device_, out_buffer, out_buffer_memory, 0 /*offset within the memory*/);
    }

    void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
    {
        // Memory transfer operations are executed using command buffers, just like drawing commands
        // -> We have to create a temporary command buffer
        // We may want to create a separate command pool for short-lived buffers so we can leverage some memory allocation optimizations.
        // For this we'd have to use the VK_COMMAND_POOL_CREATE_TRANSIENT_BIT 

        VkCommandBuffer command_buffer = BeginSingleTimeCommands();

        VkBufferCopy copy_region{};
        copy_region.srcOffset = 0; // Optional
        copy_region.dstOffset = 0; // Optional
        copy_region.size = size;
        vkCmdCopyBuffer(command_buffer, src, dst, 1, &copy_region);

        EndSingleTimeCommands(command_buffer);
    }

    void CreateImage(uint32_t width, uint32_t height, uint32_t num_mips, VkSampleCountFlagBits num_samples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                     VkImage& image, VkDeviceMemory& image_memory)
    {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = static_cast<uint32_t>(width);
        image_info.extent.height = static_cast<uint32_t>(height);
        image_info.extent.depth = 1; // One color value per texel
        image_info.mipLevels = num_mips;
        image_info.arrayLayers = 1;  // Single texture, no texture array
        image_info.format = format;
        image_info.tiling = tiling; // VK_IMAGE_TILING_LINEAR -> Texels are laid out in row-major order like the tex_data array
                                    // -> If we want to access texels directly in the memory, we have to use this!
                                    // VK_IMAGE_TILING_OPTIMAL -> Texels are laid out in an implementation defined order for optimal access
                                    // -> We use this here, because we use a staging buffer instead of a staging image
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;   // VK_IMAGE_LAYOUT_UNDEFINED -> Not usable by the GPU and the very first transition will discard the texels
                                                                // VK_IMAGE_LAYOUT_PREINITIALIZED -> Not usable by the GPU, but the first transition will preserve the texels.
                                                                // Useful if we want to use an image as a staging image, e.g. upload data to it and then transition the image to be
                                                                // a transfer source while preserving the data.
                                                                // In our case, we transition the image to be a transfer destination and then copy the texel data to it from a buffer.
        image_info.usage = usage;    // We want to transfer data to this image and we want to access the image in the shader
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Image is only used by the graphics queue family
        image_info.samples = num_samples; // Related to multisampling. Only needed if image is used as attachment.
        image_info.flags = 0; // Optional. Related to sparse images.

        if (vkCreateImage(logical_device_, &image_info, nullptr, &image) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create image!");
        }

        // Allocate memory for the image - Similar to allocating memory for a buffer
        VkMemoryRequirements mem_requirements;
        vkGetImageMemoryRequirements(logical_device_, image, &mem_requirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = mem_requirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, properties);

        if (vkAllocateMemory(logical_device_, &allocInfo, nullptr, &image_memory) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate image memory!");
        }

        vkBindImageMemory(logical_device_, image, image_memory, 0);
    }

    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t num_mips)
    {
        VkCommandBuffer command_buffer = BeginSingleTimeCommands();

        // One of the most common ways to perform layout transitions is to use an "image memory barrier" (or buffer memory barrier for buffers).
        // A pipeline barrier like that is generally used to synchronize access to resources, like ensuring that a write to a buffer completes before reading from it,
        // but it can also be used to transition image layouts and transfer queue family ownership when VK_SHARING_MODE_EXCLUSIVE is used.

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;

        barrier.oldLayout = old_layout; // use VK_IMAGE_LAYOUT_UNDEFINED if we don't care about existing contents of the image
        barrier.newLayout = new_layout;
        // NOTE: VK_IMAGE_LAYOUT_GENERAL allows all operations, but is not necessarily the most efficient layout.
        // For example, this is only needed for cases where we need to both read and write to/from an image

        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // If we use the barrier to transfer queue family ownership these fields should be the indices of the queue families
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // Otherwise set to VK_QUEUE_FAMILY_IGNORED (<- THIS IS NOT THE DEFAULT VALUE! DON'T FORGET!)

        // subresourceRange -> the specific part of the image
        barrier.subresourceRange.baseArrayLayer = 0; // The image is no array
        barrier.subresourceRange.baseMipLevel = 0;  
        barrier.subresourceRange.levelCount = num_mips;
        barrier.subresourceRange.layerCount = 1;    // -> and only 1 layer

        // Ensure proper subresource aspect is used for depth images
        if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            if (HasStencilComponent(format))
            {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        else
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        // We want to use the barrier as synchronization point
        // -> We have to specify which operations happen BEFORE the sync point and operations have to wait until AFTER the barrier
        VkPipelineStageFlags source_stage;
        VkPipelineStageFlags dest_stage;

        // There are three transitions we need to handle:
        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)  // Transfer writes that don't need to wait on anything
        {
            barrier.srcAccessMask = 0;  // writes don't have to wait on anything -> specify an empty access mask 
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;   // earliest possible pipeline stage for pre-barrier operations
            dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;    // not a "real" stage in the graphics/compute pipeline, but a pseudo stage where transfers happen.
                                                            // See https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPipelineStageFlagBits.html
        }
        else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)  // Shader reads should wait on transfer writes
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;  // image will be written in this stage
            dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // and then it'll be accessed in the fragment shader pipeline stage
        }
        else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;

            // The depth buffer will be read from to perform depth tests to see if a fragment is visible
            // It will be written to when a new fragment is drawn.
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            // Writing happens in the VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT stage
            source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

            // Reading happens in the VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT stage 
            dest_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else
        {
            throw std::invalid_argument("Unsupported layout transition!");
        }

        // Submit the barrier. (All barriers use the same function!)
        // Allowed stage values are specified here:
        // https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#synchronization-access-types-supported
        vkCmdPipelineBarrier(
            command_buffer,
            source_stage,   // <- Specify in which pipeline stage the operations occur that should happen before the barrier
            dest_stage,     // <- Specify the pipeline stage in which operations will wait on the barrier, e.g. VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT if
                            // we want to read the uniform in the fragment shader after the barrier.
            0,              // <- 0 or VK_DEPENDENCY_BY_REGION_BIT. Latter turns barrier into a per-region condition, i.e. the implementation is allowed to already begin reading parts
                            // of the resource that was already written so far.
            0, nullptr,     // <- Reference arrays of pipeline barriers of the three available types: memory barriers
            0, nullptr,     // buffer memory barriers
            1, &barrier     // and image memory barriers 
        );

        EndSingleTimeCommands(command_buffer);
    }

    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
    {
        VkCommandBuffer command_buffer = BeginSingleTimeCommands();

        // We need to specify which part of the buffer is going to be copied to which part of the image
        VkBufferImageCopy region{};
        region.bufferOffset = 0;    // Byte offset in the buffer at which the pixel values start
        region.bufferRowLength = 0;   // Specify how the pixels are laid out in memory, e.g. if we have some kind of padding bytes between rows.
        region.bufferImageHeight = 0; // 0 means image is tightly packed

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        // Which part of the image do we want to copy?
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = {
            width,
            height,
            1
        };

        vkCmdCopyBufferToImage(
            command_buffer,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // <- which layout the image is currently using
            1,
            &region
        );

        EndSingleTimeCommands(command_buffer);
    }

    // Queries the physical device for desired formats and returns the first one that's supported.
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
    {
        for (VkFormat format : candidates)
        {
            VkFormatProperties props;
            // ^^^ Fields:
            // VkFormatProperties::linearTilingFeatures - Use cases that are supported with linear tiling
            // VkFormatProperties::optimalTilingFeatures - Use cases that are supported with optimal tiling
            // VkFormatProperties::bufferFeatures - Use cases that are supported for buffers

            vkGetPhysicalDeviceFormatProperties(physical_device_, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }

        throw std::runtime_error("Failed to find supported format!");
    }

    // Helper function to select a format with depth component that is supported as depth attachment
    VkFormat FindDepthFormat()
    {
        // We have to specify the accuracy of our depth image:
        // VK_FORMAT_D32_SFLOAT: 32 - bit float for depth
        // VK_FORMAT_D32_SFLOAT_S8_UINT : 32 - bit signed float for depth and 8 bit stencil component
        // VK_FORMAT_D24_UNORM_S8_UINT : 24 - bit float for depth and 8 bit stencil component

        return FindSupportedFormat(
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    bool HasStencilComponent(VkFormat format)
    {
        // We need to take the stencil component into account when performing layout transitions on images
        // -> We have to know if we actually have one or nah
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    void CreateColorResources()
    {
        VkFormat color_format = swap_chain_image_format_;

        // Create multisampled color buffer
        CreateImage(swap_chain_extent_.width, swap_chain_extent_.height, 1, num_msaa_samples_, color_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, color_image_, color_image_memory_);
        color_image_view_ = CreateImageView(color_image_, color_format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }

    void CreateDepthResources()
    {
        VkFormat depth_format = FindDepthFormat();

        CreateImage(swap_chain_extent_.width, swap_chain_extent_.height,    // should have the same resolution as the color attachment
            1,  // No mip mapping
            num_msaa_samples_,
            depth_format,    // A format that's supported by our physical device
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, // image usage appropriate for a depth attachment
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            depth_image_, depth_image_memory_
        );

        depth_image_view_ = CreateImageView(depth_image_, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

        // Done! We don't need to map the depth image or copy another image to it, because we're going to clear it at the start of the render pass like the color attachment

        // We don't have to explicitly transition the layout of the depth image to a depth attachment because this is done in the render pass.
        // But for the sake of practicing how to do it, we'll do it now anyway :P
        
        TransitionImageLayout(depth_image_, depth_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
        // ^^^ undefined layout can be used as initial layout, because there are no existing depth image contents that matter.

    }

    void GenerateMipmaps(VkImage image, VkFormat image_format, int32_t tex_width, int32_t tex_height, uint32_t num_mips)
    {
        // Not all platforms support blitting...
        // Have to check if image format supports linear blitting first
        VkFormatProperties format_properties;
        vkGetPhysicalDeviceFormatProperties(physical_device_, image_format, &format_properties);

        // We create a texture image with the optimal tiling format, so we need to check optimalTilingFeatures
        // Blitting requires the texture image format we use to support linear filtering
        if (!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
        {
            // Alternatives:
            // 1. We could implement a function that searches common texture image formats for one that does support linear blitting
            // 2. We could implement the mipmap generation in software with a library like stb_image_resize. We'd then load the image just as we loaded the original image.
            
            // Note: Generating mipmaps at runtime is not very common. Usually they are precalculated and saved as texture next to the base texture.
            throw std::runtime_error("Texture image format does not support linear blitting!");
        }

        VkCommandBuffer command_buffer = BeginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        int32_t mip_width = tex_width;
        int32_t mip_height = tex_height;

        // vkCmdBlitImage depends on the layout of the image it operates on
        // We could use VK_IMAGE_LAYOUT_GENERAL, but this will be slow.
        // For optimal performance, the source image should be in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL and the destination image should be in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        // -> We transition each mip level independently

        for (uint32_t i = 1; i < num_mips; i++)
        {
            // transition level i - 1 to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL. 
            // This transition will wait for level i - 1 to be filled, either from the previous blit command, or from vkCmdCopyBufferToImage. 
            // The current blit command will wait on this transition.
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(command_buffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            // We use a blit command to generate the mip maps.
            // Blit -> Copy of an image + application of transforms and filters
            VkImageBlit blit{};

            // determine the 3D region that data will be blitted from. 
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mip_width, mip_height, 1 };

            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;   // the source mip level
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;

            // determines the region that data will be blitted to. 
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1, 1 }; // Divide by two because each mip lvl is half the size of the prev mip level

            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;   // the destination mip level
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            // Record the blit command
            // srcImage and dstImage are the same because we're blitting between different levels of the same image
            // TODO: once we use a dedicated transfer queue, this command must be submitted to a queue with graphics capability
            vkCmdBlitImage(command_buffer,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                VK_FILTER_LINEAR);  // VkFilter to use in the blit. Same options as VkSampler

            // Transition mip level i - 1 to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            // Transition waits on the current blit command to finish. 
            // All sampling operations will wait on this transition to finish
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(command_buffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            // Update mip extents for next iteration
            if (mip_width > 1)  // Ensure that the extents never become 0 (may happen if image is not square)
            {
                mip_width /= 2;
            }

            if (mip_height > 1)
            {
                mip_height /= 2;
            }
        }

        // Finally transition the last mip level from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        // This is necessary since the last mip level is never blitted from (and therefore we didn't do it in the blit loop before)
        barrier.subresourceRange.baseMipLevel = num_mips - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        EndSingleTimeCommands(command_buffer);
    }

    void CreateTextureImage()
    {
        int tex_width;
        int tex_height;
        int tex_channels;
        stbi_uc* tex_data = stbi_load(TEXTURE_PATH.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
        VkDeviceSize tex_size = tex_width * tex_height * 4; // 4 bytes per pixel, because we use RGBA

        if (tex_data == nullptr)
        {
            throw std::runtime_error("Failed to load texture!");
        }

        // Calculate mip levels
        num_mips_ = static_cast<uint32_t>(std::floor(std::log2(std::max(tex_width, tex_height)))) + 1;
        // ^^^
        // max: Get largest dimension
        // log2: Calculate how many times that dimension can be divided by 2
        // floor: Handle cases where the largest dimension is not a power of 2
        // Add 1 so that we have at least one mip level

        // First copy to a staging buffer
        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        CreateBuffer(tex_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);

        void* data;
        vkMapMemory(logical_device_, staging_buffer_memory, 0, tex_size, 0, &data);
        memcpy(data, tex_data, static_cast<size_t>(tex_size));
        vkUnmapMemory(logical_device_, staging_buffer_memory);

        stbi_image_free(tex_data); // We copied the data, so we don't need this anymore.

        // Then create the image object.
        // Theoretically we could use a buffer and bind it to the shader, but image objects are more performant and convenient
        // For example, we can use 2D coordinates to retrieve colors.
        CreateImage(tex_width, tex_height,
            num_mips_,
            VK_SAMPLE_COUNT_1_BIT,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_TILING_OPTIMAL, // VK_IMAGE_TILING_LINEAR -> Texels are laid out in row-major order like the tex_data array
                                     // VK_IMAGE_TILING_OPTIMAL -> Texels are laid out in an implementation defined order for optimal access
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,   // We want to copy from/to this image & we want to access it in the shader 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,    // We want most read-efficient memory type
            texture_image_, texture_image_memory_);
        
        // Now copy staging buffer to the texture image

        // First transition texture image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        TransitionImageLayout(texture_image_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_mips_);
        // ^^^ VK_IMAGE_LAYOUT_UNDEFINED, cause we don't care about the contents before performing the copy operation

        // Then execute the buffer to image copy operation
        CopyBufferToImage(staging_buffer, texture_image_, static_cast<uint32_t>(tex_width), static_cast<uint32_t>(tex_height));

        // To be able to start sampling from the texture image in the shader, we need one last transition to prepare it for shader access
        //TransitionImageLayout(texture_image_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, num_mips_);

        // This final transition is already handled in GenerateMips :)
        GenerateMipmaps(texture_image_, VK_FORMAT_R8G8B8A8_SRGB, tex_width, tex_height, num_mips_);

        // Finally clean up the buffers
        vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
        vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);
    }

    void CreateTextureImageView()
    {
        texture_image_view_ = CreateImageView(texture_image_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, num_mips_);
    }

    void CreateTextureSampler()
    {
        // Shaders CAN read directly from images, but that is not very common when they're used as textures.
        // Instead samplers are used, which apply filters and transformations before the texture is accessed.
        // Filter examples: bilinear filtering, anisotropic filtering
        // Transformation examples: What happens if we sample texels outside of the image ("adressMode")? (repeat, mirror, clamp, ...)

        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;   // How to filter textures that are magnified
        sampler_info.minFilter = VK_FILTER_LINEAR;   // How to filter textures that are minified
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;  // How do we handle sampling outside of the image boundaries?
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.anisotropyEnable = VK_TRUE;

        // Query max texels we can use for anisotropic filtering
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physical_device_, &properties);
        sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy; // Limits the amount of texel samples that can be used to calculate the final color
                                                                             // Lower value -> better performance but worse quality

        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // What color is used if we use clamping address mode? (either black, white or transparent)
        sampler_info.unnormalizedCoordinates = VK_FALSE; // True: [0, texWidth) and [0, texHeight)
                                                         // False: Normalized UV coordinates [0, 1)

        sampler_info.compareEnable = VK_FALSE;   // If enabled, texels will first be compared to a value, and the result of that comparison is used in filtering operations
                                                 // Used mainly for percentage-closer filtering https://developer.nvidia.com/gpugems/GPUGems/gpugems_ch11.html on shadow maps.
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;

        // Mip mapping settings
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f; // allow full range of mips to be used. Increase this for obvious mip mapping :)
        sampler_info.maxLod = static_cast<float>(num_mips_);

        // NOTE: The sampler does not reference a VkImage anywhere!
        // It's merely an interface to access colors from a texture.
        // Which image we sample from doesn't matter at all! Cool! :D
        if (vkCreateSampler(logical_device_, &sampler_info, nullptr, &texture_sampler_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create texture sampler!");
        }
    }

    VkSampleCountFlagBits GetMaxNumSamples()
    {
        VkPhysicalDeviceProperties physical_device_properties;
        vkGetPhysicalDeviceProperties(physical_device_, &physical_device_properties);

        // We use depth buffering, so we have to account for both color and depth samples
        VkSampleCountFlags counts = physical_device_properties.limits.framebufferColorSampleCounts & physical_device_properties.limits.framebufferDepthSampleCounts;
        if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
        if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
        if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

        return VK_SAMPLE_COUNT_1_BIT;
    }

    void LoadModel()
    {
        // An OBJ file consists of positions, normals, texture coordinates and faces. Faces consist of an arbitrary amount of vertices, 
        // where each vertex refers to a position, normal and/or texture coordinate by index. 
        // This makes it possible to not just reuse entire vertices, but also individual attributes.

        tinyobj::attrib_t attrib;
        // ^^^ Holds all positions, normals and tex coords

        std::vector<tinyobj::shape_t> shapes;
        // ^^^ Contains all of the separate objects and their faces
        // Each face consists of an array of vertices, and each vertex contains the indices of the position, normal and texture coordinate attributes

        std::vector<tinyobj::material_t> materials;
        // ^^^ OBJ models can also define a material and texture per face

        std::string warn, err;

        // Load the model with tinyobj
        // LoadObj automatically triangulates the vertices by default
        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str()))
        {
            throw std::runtime_error(warn + err);
        }

        // Fill vertices / indices array from loaded data
        std::unordered_map<Vertex, uint32_t> unique_vertices{};

        for (const auto& shape : shapes)
        {
            for (const auto& index : shape.mesh.indices)
            {
                Vertex vertex{};

                // Look up the actual vertex attributes in the attrib arrays
                // We can be sure that every face has 3 vertices.
                // This is a flat array though, so we have to use a stride of 3
                vertex.pos_ = 
                {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                // Similarly, every vertex has 2 texcoord values
                vertex.tex_coords_ = 
                {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]   // OBJ format assumes 0 is at bottom, but it's actually at the top in our case
                };

                vertex.color_ = { 1.0f, 1.0f, 1.0f };

                // Only keep unique vertices so we can make use of the index buffer
                if(unique_vertices.count(vertex) == 0)
                {
                    uint32_t vertex_index = static_cast<uint32_t>(vertices_.size());
                    unique_vertices[vertex] = vertex_index;
                    vertices_.push_back(vertex);
                }

                indices_.push_back(unique_vertices[vertex]);
            }
        }
    }

    void CreateVertexBuffer()
    {
        VkDeviceSize buffer_size = sizeof(vertices_[0]) * vertices_.size();

        // Use host-visible buffer as temporary staging buffer, which is later copied to device local memory.
        // Device local memory is optimal for reading speed on the GPU, but not accessible from the CPU!
        // To copy to device local memory we therefore can't use vkMapMemory.
        // Instead we have to specify the VK_BUFFER_USAGE_TRANSFER_SRC_BIT or VK_BUFFER_USAGE_TRANSFER_DST_BIT properties.
        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);
        // ^^^ Properties
        // VK_BUFFER_USAGE_TRANSFER_SRC_BIT -> Buffer can be used as source in a memory transfer operation.
        // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT -> We want to write to the vertex buffer from the CPU
        // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT -> Makes sure that data is directly written to memory
        // otherwise the writes may be cached first and are subsequently not directly available.
        // This may cost some performance
        // Alternatively we could also call vkFlushMappedMemoryRanges after writing or
        // vkInvalidateMappedMemoryRanges before reading mapped memory.

        // Map allocated memory into CPU address space, copy over vertices to staging buffer
        void* data;
        vkMapMemory(logical_device_, staging_buffer_memory, 0 /*offset*/, buffer_size, 0 /*additional flags. Has to be 0.*/, &data);
        memcpy(data, vertices_.data(), (size_t) buffer_size);    // No flush required as we set VK_MEMORY_PROPERTY_HOST_COHERENT_BIT.
        vkUnmapMemory(logical_device_, staging_buffer_memory);

        CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertex_buffer_, vertex_buffer_memory_);
        // ^^^
        // VK_BUFFER_USAGE_TRANSFER_DST_BIT -> Buffer can be used as destination in a memory transfer operation.

        CopyBuffer(staging_buffer, vertex_buffer_, buffer_size);

        // Once the copy command is done we can clean up the staging buffer
        vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
        vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);
    }

    void CreateIndexBuffer()
    {
        // Basically same as CreateVertexBuffer, but now we create a buffer for the indices.
        // Notice the VK_BUFFER_USAGE_INDEX_BUFFER_BIT
         
        VkDeviceSize buffer_size = sizeof(indices_[0]) * indices_.size();

        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);

        void* data;
        vkMapMemory(logical_device_, staging_buffer_memory, 0, buffer_size, 0, &data);
        memcpy(data, indices_.data(), (size_t)buffer_size);
        vkUnmapMemory(logical_device_, staging_buffer_memory);

        CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, index_buffer_, index_buffer_memory_);

        CopyBuffer(staging_buffer, index_buffer_, buffer_size);

        vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
        vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);
    }

    void CreateUniformBuffers()
    {
        VkDeviceSize buffer_size = sizeof(UniformBufferObject);

        // We should not modify the uniforms of a frame that is in-flight!
        // -> We need one uniform buffer per swap chain image.
        uniform_buffers_.resize(swap_chain_images_.size());
        uniform_buffers_memory_.resize(swap_chain_images_.size());
        for (size_t i = 0; i < swap_chain_images_.size(); i++)
        {
            // Since the uniform data is updated every frame, a staging buffer would only add unnecessary overhead.
            CreateBuffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniform_buffers_[i], uniform_buffers_memory_[i]);
        }
    }

    void CreateDescriptorPool()
    {
        // IMPORTANT: Inadequate descriptor pools are a good example of a problem that the validation layers will not catch
        // vkAllocateDescriptorSets may fail with the error code VK_ERROR_POOL_OUT_OF_MEMORY if the pool is not sufficiently large,
        // but the driver may also try to solve the problem internally. Sometimes we get away with exceeding the limits of the descriptor pool,
        // othertimes it fails - depending on the user's hardware.
        // This makes bugs like this hard to catch, so keep this in mind!
        
        std::array<VkDescriptorPoolSize, 2> pool_sizes{};
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = static_cast<uint32_t>(swap_chain_images_.size());   // allocate one descriptor for every swap chain image
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[1].descriptorCount = static_cast<uint32_t>(swap_chain_images_.size());

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());;
        pool_info.pPoolSizes = pool_sizes.data();
        pool_info.maxSets = static_cast<uint32_t>(swap_chain_images_.size());

        if (vkCreateDescriptorPool(logical_device_, &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
    }

    void CreateDescriptorSets()
    {
        std::vector<VkDescriptorSetLayout> layouts(swap_chain_images_.size(), descriptor_set_layout_);
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = descriptor_pool_;
        alloc_info.descriptorSetCount = static_cast<uint32_t>(swap_chain_images_.size());
        alloc_info.pSetLayouts = layouts.data();

        // Create one descriptor set for each swap chain image.
        descriptor_sets_.resize(swap_chain_images_.size());
        if (vkAllocateDescriptorSets(logical_device_, &alloc_info, descriptor_sets_.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }

        // Then populate the descriptors inside of the descriptor sets
        for (size_t i = 0; i < swap_chain_images_.size(); i++)
        {
            VkDescriptorBufferInfo buffer_info{};
            buffer_info.buffer = uniform_buffers_[i];
            buffer_info.offset = 0;
            buffer_info.range = sizeof(UniformBufferObject);

            VkDescriptorImageInfo image_info{};
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView = texture_image_view_;
            image_info.sampler = texture_sampler_;

            std::array<VkWriteDescriptorSet, 2> descriptor_writes{};

            descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[0].dstSet = descriptor_sets_[i];  // the descriptor set to update
            descriptor_writes[0].dstBinding = 0;    // Binding index
            descriptor_writes[0].dstArrayElement = 0;   // descriptors can be arrays -> Have to specify the first index
            descriptor_writes[0].descriptorCount = 1;   // How many descriptors in the array we want to update.
            descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;    // Need to specify the type of descriptor again
            descriptor_writes[0].pBufferInfo = &buffer_info;    // used for descriptors that refer to buffer data
            descriptor_writes[0].pImageInfo = nullptr; // used for descriptors that refer to image data
            descriptor_writes[0].pTexelBufferView = nullptr; // used for descriptors that refer to buffer views

            descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[1].dstSet = descriptor_sets_[i];
            descriptor_writes[1].dstBinding = 1;
            descriptor_writes[1].dstArrayElement = 0;
            descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_writes[1].descriptorCount = 1;
            descriptor_writes[1].pImageInfo = &image_info;

            vkUpdateDescriptorSets(logical_device_, static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data(), 0, nullptr /*can be used to copy descriptors to each other*/);
        }

    }

    void UpdateUniformData(uint32_t current_swap_chain_img_idx)
    {
        static auto start_time = std::chrono::high_resolution_clock::now();

        auto current_time = std::chrono::high_resolution_clock::now();

        // Time in sec since rendering started
        float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

        UniformBufferObject ubo{};

        // Rotate around the z-axis
        ubo.model = glm::rotate(
            glm::mat4(1.0f),    // Existing transform. In this case identity.
            time * glm::radians(90.0f), // Rotation angle -> In this case 90 degrees per second
            glm::vec3(0.0f, 0.0f, 1.0f) // Rotation axis
        );

        // Look at the model from above at 45° angle
        ubo.view = glm::lookAt(
            glm::vec3(2.0f, 2.0f, 2.0f),    // Eye pos
            glm::vec3(0.0f, 0.0f, 0.0f),    // Center pos
            glm::vec3(0.0f, 0.0f, 1.0f)     // Up direction
        );

        ubo.proj = glm::perspective(
            glm::radians(45.0f),    // FoV
            swap_chain_extent_.width / static_cast<float>(swap_chain_extent_.height),  // Aspect ratio.
            0.1f,   // Near plane
            10.0f   // Far plane
        );

        // GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted.
        // -> Have to flip sign on the scaling factor of the Y axis in the projection matrix.
        // If we don't do this, then the image will be rendered upside down.
        ubo.proj[1][1] *= -1;

        // Finally copy data into the uniform buffer
        // This is not the most efficient way to pass frequently changing values to a shader.
        // Check out "Push constants" for more info!
        void* data;
        vkMapMemory(logical_device_, uniform_buffers_memory_[current_swap_chain_img_idx], 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(logical_device_, uniform_buffers_memory_[current_swap_chain_img_idx]);
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
        VkResult result = vkAcquireNextImageKHR(logical_device_, swap_chain_, UINT64_MAX /*disable time out*/, image_available_semaphores_[current_frame_], VK_NULL_HANDLE, &image_index);

        // Check for window resizes, so we can recreate the swap chain.
        // VK_ERROR_OUT_OF_DATE_KHR -> Swap chain is incompatible with the surface. Typically happens on window resize, but not guaranteed.
        // VK_SUBOPTIMAL_KHR -> Some parts of the swap chain are incompatible, but we could theoretically still present to the surface.
        if(result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateSwapChain();
            return;
        }
        else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("Failed to acquire swap chain image");
        }

        // If MAX_FRAMES_IN_FLIGHT is higher than the number of swap chain images or vkAcquireNextImageKHR returns images out-of-order 
        // it's possible that we may start rendering to a swap chain image that is already in flight.
        // To avoid this, we need to track for each swap chain image if a frame in flight is currently using it.
        if (inflight_images_[image_index] != VK_NULL_HANDLE)
        {
            vkWaitForFences(logical_device_, 1, &inflight_images_[image_index], VK_TRUE, UINT64_MAX);
        }

        // Mark the image as now being in use by this frame
        inflight_images_[image_index] = inflight_frame_fences_[current_frame_];

        UpdateUniformData(image_index);

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
        result = vkQueuePresentKHR(present_queue_, &present_info);

        // Explicitly check for window resize, so we can recreate the swap chain.
        // In this case it's important to do this after present to ensure that the semaphores are in the correct state.
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || was_frame_buffer_resized_)
        {
            was_frame_buffer_resized_ = false;
            RecreateSwapChain();
        }
        else if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to present swap chain image to surface");
        }

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

    const std::string MODEL_PATH = "assets/models/viking_room.obj";
    const std::string TEXTURE_PATH = "assets/textures/viking_room.png";

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

    VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;  // Combination of all descriptor bindings
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline_ = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> swap_chain_framebuffers_;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers_;

    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> inflight_frame_fences_;
    std::vector<VkFence> inflight_images_;

    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;

    VkBuffer vertex_buffer_;
    VkDeviceMemory vertex_buffer_memory_;
    VkBuffer index_buffer_;
    VkDeviceMemory index_buffer_memory_;

    std::vector<VkBuffer> uniform_buffers_;
    std::vector<VkDeviceMemory> uniform_buffers_memory_;    // Array, because we need one uniform buffer per swap chain image!

    VkDescriptorPool descriptor_pool_;
    std::vector<VkDescriptorSet> descriptor_sets_;

    uint32_t num_mips_;
    VkImage texture_image_;
    VkDeviceMemory texture_image_memory_;
    VkImageView texture_image_view_;
    VkSampler texture_sampler_;

    // Depth attachment
    VkImage depth_image_;    // Only need one, because only one draw operation is executed at a time.
    VkDeviceMemory depth_image_memory_;
    VkImageView depth_image_view_;

    // MSAA
    VkSampleCountFlagBits num_msaa_samples_ = VK_SAMPLE_COUNT_1_BIT; // By default we'll be using only one sample per pixel -> no multisampling
    VkImage color_image_;   // offscreen buffer we sample from
    VkDeviceMemory color_image_memory_;
    VkImageView color_image_view_;

    uint32_t current_frame_ = 0;
    bool was_frame_buffer_resized_ = false;
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
