#pragma once

#define GLM_FORCE_RADIANS   // Ensure that matrix functions use radians as units
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // GLM perspective projection matrix will use depth range of -1.0 to 1.0 by default. We need range of 0.0 to 1.0 for Vulkan.
#define GLM_ENABLE_EXPERIMENTAL // Needed so we can use the hash functions of GLM types
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // matrix functions like glm::lookAt etc.

#include "Vertex.h"

struct GLFWwindow;

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

class VulkanRenderer
{
public:
    void Init(GLFWwindow* window);
    void OnFrameBufferResize();
    void DrawFrame();
    void Cleanup();

private:

    void InitVulkan();

    void MainLoop();

    void CreateVulkanInstance();

    std::vector<const char*> GetRequiredExtensions();

    bool CheckInstanceExtensionSupport(const std::vector<const char*>& required_extensions);

    void SetupDebugManager();

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data);

    void CreateSurface();

    void SelectPhysicalDevice();

    bool CheckDeviceRequirements(VkPhysicalDevice device);

    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

    void CreateLogicalDevice();

    void CreateSwapChain();

    void CleanUpSwapChain();

    // Recreate SwapChain and all things depending on it.
    void RecreateSwapChain();

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats);

    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes);

    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t num_mips);

    void CreateImageViews();

    bool CheckValidationLayerSupport();

    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info);

    void CreateRenderPass();

    void CreateDescriptorSetLayout();

    void CreateGraphicsPipeline();

    VkShaderModule CreateShaderModule(const std::vector<char>& code);

    void CreateFramebuffers();

    void CreateCommandPool();

    VkCommandBuffer BeginSingleTimeCommands();

    void EndSingleTimeCommands(VkCommandBuffer command_buffer);

    void CreateCommandBuffers();

    uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);

    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& out_buffer, VkDeviceMemory& out_buffer_memory);

    void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

    void CreateImage(uint32_t width, uint32_t height, uint32_t num_mips, VkSampleCountFlagBits num_samples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
        VkImage& image, VkDeviceMemory& image_memory);

    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t num_mips);

    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    // Queries the physical device for desired formats and returns the first one that's supported.
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    // Helper function to select a format with depth component that is supported as depth attachment
    VkFormat FindDepthFormat();

    bool HasStencilComponent(VkFormat format);

    void CreateColorResources();

    void CreateDepthResources();

    void GenerateMipmaps(VkImage image, VkFormat image_format, int32_t tex_width, int32_t tex_height, uint32_t num_mips);

    void CreateTextureImage();

    void CreateTextureImageView();

    void CreateTextureSampler();

    VkSampleCountFlagBits GetMaxNumSamples();

    void LoadModel();

    void CreateVertexBuffer();

    void CreateIndexBuffer();

    void CreateUniformBuffers();

    void CreateDescriptorPool();

    void CreateDescriptorSets();

    void UpdateUniformData(uint32_t current_swap_chain_img_idx);

    void CreateSyncObjects();

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
