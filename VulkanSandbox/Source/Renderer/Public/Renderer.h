#pragma once

#define GLM_FORCE_RADIANS   // Ensure that matrix functions use radians as units
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // GLM perspective projection matrix will use depth range of -1.0 to 1.0 by default. We need range of 0.0 to 1.0 for Vulkan.
#define GLM_ENABLE_EXPERIMENTAL // Needed so we can use the hash functions of GLM types
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // matrix functions like glm::lookAt etc.

#include "Vertex.h"
#include "VulkanBuffer.h"
#include "VulkanRenderPass.h"
#include "VulkanContext.h"
#include "VulkanViewport.h"
#include "VulkanCommandBufferPool.h"
#include "VulkanCommandBuffer.h"

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

class VulkanRenderer
{
public:
    void Init(VulkanContext* RHI, GLFWwindow* window);
    void OnFrameBufferResize(uint32_t width, uint32_t height);
    void DrawFrame();
    void Cleanup();

private:

    std::vector<const char*> GetRequiredExtensions();

    bool CheckInstanceExtensionSupport(const std::vector<const char*>& required_extensions);

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data);

    void CreateSurface(GLFWwindow* window);

    void CleanUpSwapChain();

    // Recreate SwapChain and all things depending on it.
    void RecreateSwapChain();

    void CreateDescriptorSetLayout();

    void CreateGraphicsPipeline();

    VkShaderModule CreateShaderModule(const std::vector<char>& code);

    void CreateFramebuffers();

    VkCommandBuffer BeginSingleTimeCommands();

    void EndSingleTimeCommands(VkCommandBuffer command_buffer);

    void FillCommandBuffers();

    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& out_buffer, VkDeviceMemory& out_buffer_memory);

    void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t num_mips);

    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

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

    uint32_t framebuffer_width_ = 0;
    uint32_t framebuffer_height_ = 0;

    const std::string MODEL_PATH = "assets/models/viking_room.obj";
    const std::string TEXTURE_PATH = "assets/textures/viking_room.png";

    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;

    const std::vector<const char*> valiation_layers_ = { "VK_LAYER_KHRONOS_validation" };
#ifdef NDEBUG
    const bool enable_validation_layers_ = false;
#else
    const bool enable_validation_layers_ = true;
#endif

    const std::vector<const char*> device_extensions_ = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };    // Availability of a present queue implicitly ensures that swapchains are supported
                                                                                                // but being explicit is good practice. Also we have to explicitly enable the extension
                                                                                                // anyway...

    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    // Used by a pipeline to access the descriptor sets.
    // Defines interface between shader stages used by the pipeline and shader resources (but doesn't do the actual binding!)
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;

    // Pipeline State Object. Used to bake all states that affect a pipeline.
    // VK requires us to specify graphics and compute pipelines upfront.
    // -> We have to create a new pipeline for each combination of pipeline states.
    VkPipeline graphics_pipeline_ = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> swap_chain_framebuffers_;

    VulkanCommandBufferPool* command_buffer_pool_ = nullptr;
    std::vector<VulkanCommandBuffer*> command_buffers_;

    // Semaphores - Device-internal synchronizations. Coordinate operations within the graphics queue, ensure correct command ordering
    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;

    // Fences - Host-Device-Synchronization. Use to check completion of queued operations, e.g. command buffer executions.
    std::vector<VkFence> inflight_frame_fences_;
    std::vector<VkFence> inflight_images_;

    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;

    VulkanBuffer vertex_buffer_;
    VulkanBuffer index_buffer_;

    std::vector<VkBuffer> uniform_buffers_;
    std::vector<VkDeviceMemory> uniform_buffers_memory_;    // Array, because we need one uniform buffer per swap chain image!

    VkDescriptorPool descriptor_pool_;

    // Describes shader binding layout, without referencing any descriptor sets
    // Similar to pipeline layout, it's only a blueprint.
    VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;

    // Stores which resources are bound to binding points in a shader.
    // -> Glue between shaders and the actual resources.
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

    VulkanContext* RHI_ = nullptr;
    VulkanViewport* viewport_ = nullptr;
    VulkanRenderPass* render_pass_ = nullptr;
};
