#include "Renderer.h"

#include <chrono>

#define GLFW_INCLUDE_VULKAN // GLFW will load vulkan.h
#include <GLFW/glfw3.h>

#include <stb/stb_image.h>
#include <tiny_obj_loader.h>

#include "FileIO.h"
#include "VulkanDevice.h"
#include "VulkanMacros.h"
#include "VulkanQueue.h"
#include "VulkanContext.h"
#include "VulkanViewport.h"

void VulkanRenderer::OnFrameBufferResize(uint32_t width, uint32_t height)
{
    was_frame_buffer_resized_ = true;
    framebuffer_width_ = width;
    framebuffer_height_ = height;
}

void VulkanRenderer::Init(VulkanContext* RHI, GLFWwindow* window)
{
    CHECK(window != nullptr);
    RHI_ = RHI;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    framebuffer_width_ = static_cast<uint32_t>(width);
    framebuffer_height_ = static_cast<uint32_t>(height);

    // A surface represents an abstract type to present rendered images to. The surface in our program will be backed by the window that we've already opened with GLFW.
    // We have to create a surface before we select the physical device to ensure that the device meets our requirements.
    CreateSurface(window);
    RHI->GetDevice()->InitPresentQueue(surface_);

    viewport_ = new VulkanViewport(RHI_, surface_, framebuffer_width_, framebuffer_height_);

    // Tell Vulkan about the framebuffer attachments that will be used while rendering
    // e.g. how many color and depth buffers there will be, how many samples to use for each of them,
    // how their contents should be handled throughout the rendering, operations,...
    render_pass_ = new VulkanRenderPass(RHI, viewport_->GetSwapChain());

    // Specify the types of resources that are going to be accessed by the pipeline
    CreateDescriptorSetLayout();

    CreateGraphicsPipeline(); // Configure stages of the render pipeline

    command_buffer_pool_ = new VulkanCommandBufferPool(RHI->GetDevice());

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

    // Create command buffers
    // Because one of the drawing commands involves binding the right VkFramebuffer, we have to record a command buffer for every image in the swap chain.
    command_buffers_.resize(swap_chain_framebuffers_.size());
    for (size_t i = 0; i < command_buffers_.size(); ++i)
    {
        command_buffers_[i] = new VulkanCommandBuffer(RHI->GetDevice(), command_buffer_pool_);
    }

    // For now also prerecord the command buffers since we want to show a static model
    FillCommandBuffers();

    CreateSyncObjects();

    LOG("VulkanRenderer::Init - Finished");
}

void VulkanRenderer::Cleanup()
{
    // operations in drawFrame are asynchronous -> When we exit the loop there may still be some ongoing operations and we shouldn't destroy the resources until we are done using those.
    // => Wait for the logical device to finish operations before cleaning up.
    RHI_->GetDevice()->WaitUntilIdle();

    CleanUpSwapChain();

    vkDestroySampler(RHI_->GetDevice()->GetLogicalDeviceHandle(), texture_sampler_, nullptr);
    vkDestroyImageView(RHI_->GetDevice()->GetLogicalDeviceHandle(), texture_image_view_, nullptr);

    vkDestroyImage(RHI_->GetDevice()->GetLogicalDeviceHandle(), texture_image_, nullptr);
    vkFreeMemory(RHI_->GetDevice()->GetLogicalDeviceHandle(), texture_image_memory_, nullptr);

    vkDestroyDescriptorSetLayout(RHI_->GetDevice()->GetLogicalDeviceHandle(), descriptor_set_layout_, nullptr);

    // Destroy buffers and corresponding memory
    index_buffer_.Destroy();
    vertex_buffer_.Destroy();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(RHI_->GetDevice()->GetLogicalDeviceHandle(), render_finished_semaphores_[i], nullptr);
        vkDestroySemaphore(RHI_->GetDevice()->GetLogicalDeviceHandle(), image_available_semaphores_[i], nullptr);
        vkDestroyFence(RHI_->GetDevice()->GetLogicalDeviceHandle(), inflight_frame_fences_[i], nullptr);
    }

    delete command_buffer_pool_;
    command_buffer_pool_ = nullptr;

    vkDestroySurfaceKHR(RHI_->GetInstance().GetHandle(), surface_, nullptr);
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
    std::cerr << "Validation layer: " << callback_data->pMessage << std::endl;

    // Return value indicates if the Vulkan call that triggered the validation layer message should be aborted with VK_ERROR_VALIDATION_FAILED_EXT.
    // Usually this is only used to test validation layers. -> We should most likely always return VK_FALSE here.
    return VK_FALSE;
}

void VulkanRenderer::CreateSurface(GLFWwindow* window)
{
    assert(window != nullptr);

    // glfw offers a handy abstraction for surface creation.
    // It automatically fills a VkWin32SurfaceCreateInfoKHR struct with the platform specific window and process handles
    // and then calls the platform specific function to create the surface, e.g. vkCreateWin32SurfaceKHR
    if (glfwCreateWindowSurface(RHI_->GetInstance().GetHandle(), window, nullptr, &surface_) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create window surface!");
    }
}

void VulkanRenderer::CleanUpSwapChain()
{
    // multisampled color buffer (MSAA)
    vkDestroyImageView(RHI_->GetDevice()->GetLogicalDeviceHandle(), color_image_view_, nullptr);
    vkDestroyImage(RHI_->GetDevice()->GetLogicalDeviceHandle(), color_image_, nullptr);
    vkFreeMemory(RHI_->GetDevice()->GetLogicalDeviceHandle(), color_image_memory_, nullptr);

    // depth buffer
    vkDestroyImageView(RHI_->GetDevice()->GetLogicalDeviceHandle(), depth_image_view_, nullptr);
    vkDestroyImage(RHI_->GetDevice()->GetLogicalDeviceHandle(), depth_image_, nullptr);
    vkFreeMemory(RHI_->GetDevice()->GetLogicalDeviceHandle(), depth_image_memory_, nullptr);

    for (auto framebuffer : swap_chain_framebuffers_)
    {
        vkDestroyFramebuffer(RHI_->GetDevice()->GetLogicalDeviceHandle(), framebuffer, nullptr);
    }

    // We don't have to recreate the whole command pool.
    for(size_t i=0; i<command_buffers_.size(); ++i)
    {
        delete command_buffers_[i];
        command_buffers_[i] = nullptr;
    }

    vkDestroyPipeline(RHI_->GetDevice()->GetLogicalDeviceHandle(), graphics_pipeline_, nullptr);
    vkDestroyPipelineLayout(RHI_->GetDevice()->GetLogicalDeviceHandle(), pipeline_layout_, nullptr);

    delete render_pass_;
    render_pass_ = nullptr;

    viewport_->DestroySwapchain();

    // Clean up uniform buffer here, as it depends on the number of images in the swap chain.
    for (size_t i = 0; i < viewport_->GetSwapChain()->GetSwapChainImages().size(); i++)
    {
        vkDestroyBuffer(RHI_->GetDevice()->GetLogicalDeviceHandle(), uniform_buffers_[i], nullptr);
        vkFreeMemory(RHI_->GetDevice()->GetLogicalDeviceHandle(), uniform_buffers_memory_[i], nullptr);
    }

    // The descriptor pool also depends on the number of swap chain images
    vkDestroyDescriptorPool(RHI_->GetDevice()->GetLogicalDeviceHandle(), descriptor_pool_, nullptr);
}

void VulkanRenderer::RecreateSwapChain()
{
    // TODO: Refactoring.
    CHECK_NO_ENTRY();

    /* 
    // In case we minimize the frame buffer will have size 0.
    // -> We pause the application until it has a frame buffer with a valid size again.
    // TODO: Get rid of the glfw stuff. Ideally the renderer should be agnostic to glfw / sdl or whatever we use to create the window.
    while (framebuffer_width_ == 0 || framebuffer_height_ == 0)
    {
        glfwWaitEvents();
    }

    // Wait until resources aren't used anymore
    RHI_->GetDevice()->WaitUntilIdle();

    // ^^^ This kinda sucks, because we have to stop rendering in order to recreate the swap chain.
    // We could pass the old swap chain object to the vkSwapchainCreateInfoKHR struct and then destroy the old swap chain
    // as soon as we're finished with it.

    // Clean up old objects
    CleanUpSwapChain();

    // Then recreate swap chain itself, and subsequently everything that depends on it
    //CreateSwapChain();
    //CreateImageViews(); // -> Are based directly on the swap chain images

    // Render pass depends on the format of the swap chain (format probably won't change, but it doesn't hurt to handle this case)
    CHECK(render_pass_ == nullptr);
    render_pass_ = new VulkanRenderPass(RHI_, viewport_->GetSwapChain());

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
     */
}

void VulkanRenderer::CreateDescriptorSetLayout()
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

    if (vkCreateDescriptorSetLayout(RHI_->GetDevice()->GetLogicalDeviceHandle(), &layout_info, nullptr, &descriptor_set_layout_) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }
}

void VulkanRenderer::CreateGraphicsPipeline()
{
    // Load shader byte code
    auto vs_source = FileIO::ReadFile("assets/shaders/vert.spv");
    auto fs_source = FileIO::ReadFile("assets/shaders/frag.spv");

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
    viewport.width = static_cast<float>(viewport_->GetSwapChain()->GetImageExtent().width);
    viewport.height = static_cast<float>(viewport_->GetSwapChain()->GetImageExtent().height);
    viewport.minDepth = 0.0f;   // must be in range [0.0, 1.0]
    viewport.maxDepth = 1.0f;   // must be in range [0.0, 1.0]

    // Any pixels outside the scissor rectangles will be discarded by the rasterizer.
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = viewport_->GetSwapChain()->GetImageExtent();

    // Combine both viewport and scissor rect into a viewport state
    VkPipelineViewportStateCreateInfo viewport_state_info{};
    viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_info.viewportCount = 1;  // Some GPUs support multiple
    viewport_state_info.pViewports = &viewport;
    viewport_state_info.scissorCount = 1;   // Some GPUs support multiple
    viewport_state_info.pScissors = &scissor;

    // Rasterizer: Turn geometry output from vertex shader and turn it into fragments to be colored by the fragment shader.
    // Also performs depth testing, face culling and the scissor test.
    // Can be configured to output fragments that fill entire polygons or just the edges (i.e. wireframe rendering). 
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

    if (vkCreatePipelineLayout(RHI_->GetDevice()->GetLogicalDeviceHandle(), &pipeline_layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS)
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
    pipeline_create_info.renderPass = render_pass_->GetHandle();
    pipeline_create_info.subpass = 0;   // index of the sub pass where this graphics pipeline will be used
    pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;   // Optional. Vulkan allows creation of a new graphics pipeline by deriving from an existing pipeline
                                                                // Deriving is less expensive to set up when pipelines have lots of functionality in common and
                                                                // switching between pipelines from the same parent can be done quicker.
    pipeline_create_info.basePipelineIndex = -1; // Optional

    // Time to create the graphics pipeline!
    uint32_t create_info_count = 1; // We could create multiple render pipelines at once.
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;    // can be used to store and reuse data relevant to pipeline creation across multiple calls to vkCreateGraphicsPipelines
                                                        // and even across program executions if the cache is stored to a file. 
    if (vkCreateGraphicsPipelines(RHI_->GetDevice()->GetLogicalDeviceHandle(), pipeline_cache, create_info_count, &pipeline_create_info, nullptr, &graphics_pipeline_) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    // Finally clean up the shader modules
    vkDestroyShaderModule(RHI_->GetDevice()->GetLogicalDeviceHandle(), frag_shader_module, nullptr);
    vkDestroyShaderModule(RHI_->GetDevice()->GetLogicalDeviceHandle(), vert_shader_module, nullptr);
}

VkShaderModule VulkanRenderer::CreateShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data()); // Have to reinterpret cast here because we got char* but uint32_t* is expected.

    VkShaderModule shader_module;
    if (vkCreateShaderModule(RHI_->GetDevice()->GetLogicalDeviceHandle(), &create_info, nullptr, &shader_module) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module!");
    }

    return shader_module;
}

void VulkanRenderer::CreateFramebuffers()
{
    // Create frame buffer for each image view in our swap chain
    swap_chain_framebuffers_.resize(viewport_->GetBackBufferImageViews().size());
    for (size_t i = 0; i < viewport_->GetBackBufferImageViews().size(); i++)
    {
        // This has to be in the correct order, as specified in the render pass!
        std::array<VkImageView, 3> attachments =
        {
            color_image_view_,
            depth_image_view_,  // depth buffer can be used by all of the swap chain images, because only a single subpass is running at the same time
            viewport_->GetBackBufferImageViews()[i] // Color attachment differs for every swap chain image
        };

        VkFramebufferCreateInfo frambuffer_info{};
        frambuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frambuffer_info.renderPass = render_pass_->GetHandle();  // Framebuffer needs to be compatible with this render pass -> Use same number and types of attachments
        frambuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        frambuffer_info.pAttachments = attachments.data(); // Specify the VkImageView objects that should be bound to the respective attachment descriptions
                                                           // in the render pass pAttachment array.
        frambuffer_info.width = viewport_->GetWidth();
        frambuffer_info.height = viewport_->GetHeight();
        frambuffer_info.layers = 1; // swap chain images are single images -> 1 layer.

        if (vkCreateFramebuffer(RHI_->GetDevice()->GetLogicalDeviceHandle(), &frambuffer_info, nullptr, &swap_chain_framebuffers_[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }
}

VkCommandBuffer VulkanRenderer::BeginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_buffer_pool_->GetHandle();
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(RHI_->GetDevice()->GetLogicalDeviceHandle(), &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}

void VulkanRenderer::EndSingleTimeCommands(VkCommandBuffer command_buffer)
{
    vkEndCommandBuffer(command_buffer);

    // Submit the command buffer to complete the operations
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(RHI_->GetDevice()->GetGraphicsQueue()->GetHandle(), 1, &submit_info, VK_NULL_HANDLE);

    // Execute transfer immediately. We could use a fence to wait for this to be executed
    // or we simply wait for the transfer queue to be idle.
    // -> A fence would allow us to schedule multiple transfers at the same time instead of doing one transfer at a time.
    // -> There's more room for performance optimizations
    vkQueueWaitIdle(RHI_->GetDevice()->GetGraphicsQueue()->GetHandle());

    // TODO: 
    // Combine these operations in a single command buffer and execute them asynchronously for higher throughput, especially the transitions and copy in the createTextureImage function.
    // Try to experiment with this by creating a setupCommandBuffer that the helper functions record commands into, and add a flushSetupCommands to execute the commands that have 
    // been recorded so far. It's best to do this after the texture mapping works to check if the texture resources are still set up correctly.

    // Once the transfer is done we can clean up.
    vkFreeCommandBuffers(RHI_->GetDevice()->GetLogicalDeviceHandle(), command_buffer_pool_->GetHandle(), 1, &command_buffer);
}

void VulkanRenderer::FillCommandBuffers()
{
    for (size_t i = 0; i < command_buffers_.size(); ++i)
    {
        CHECK(command_buffers_[i] != nullptr);
        VkCommandBuffer cmd_buffer = command_buffers_[i]->GetHandle();

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;   // Optional.
                                // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will be rerecorded right after executing it once.
                                // VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: This is a secondary command buffer that will be entirely within a single render pass.
                                // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: The command buffer can be resubmitted while it is also already pending execution.
        begin_info.pInheritanceInfo = nullptr;  // Optional. Specifies which state to inherit from the calling primary command buffers.
                                                // Only relevant for secondary command buffers.
        VERIFY_VK_RESULT(vkBeginCommandBuffer(cmd_buffer, &begin_info));

        VkRenderPassBeginInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = render_pass_->GetHandle();
        render_pass_info.framebuffer = swap_chain_framebuffers_[i];
        render_pass_info.renderArea.offset = { 0, 0 };
        render_pass_info.renderArea.extent = {viewport_->GetWidth(), viewport_->GetHeight()};    // Pixels outside this region will have undefined values.
                                                                    // It should match the size of the attachments for best performance.

        // define the clear values to use for VK_ATTACHMENT_LOAD_OP_CLEAR
        // IMPORTANT: order of clear_values should be identical to the order of attachments
        std::array<VkClearValue, 2> clear_values{};
        clear_values[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
        clear_values[1].depthStencil = { 1.0f, 0 }; // 0.0 is at the near view plane, 1.0 lies at the far view plane.
                                                    // Initial value should be furthest possible depth, i.e. 1.0

        render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());;
        render_pass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(cmd_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);

        // We've now told Vulkan which operations to execute in the graphics pipeline and which attachment to use in the fragment shader,
        // so all that remains is binding the vertex buffer and drawing the triangle
        VkBuffer vertex_buffers[] = { vertex_buffer_.GetBufferHandle() };
        VkDeviceSize offsets[] = { 0 };

        // Bind vertex buffer to bindings
        vkCmdBindVertexBuffers(cmd_buffer, 0 /*offset*/, 1 /*num bindings*/,
            vertex_buffers, offsets /*byte offsets to start reading the data from*/);
        vkCmdBindIndexBuffer(cmd_buffer, index_buffer_.GetBufferHandle(), 0 /*offset*/, VK_INDEX_TYPE_UINT32);   // We can only bind one index buffer!
                                                                                                        // Can't use different indices for each vertex attribute (e.g. for normals)
                                                                                                        // Also: If we have uint32 indices, we have to adjust the type!

        // Bind descriptor set to the descriptors in the shader
        vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, // <- have to specify if we bind to graphics or compute pipeline
            pipeline_layout_, 0, 1, &descriptor_sets_[i], 0, nullptr);

        //vkCmdDraw(command_buffers_[i], static_cast<uint32_t>(vertices.size()), 1, 0, 0);  // <-- Draws without index buffer
        vkCmdDrawIndexed(cmd_buffer, static_cast<uint32_t>(indices_.size()), 1, 0, 0, 0); // <- Draws with index buffer

        vkCmdEndRenderPass(cmd_buffer);

        VERIFY_VK_RESULT(vkEndCommandBuffer(cmd_buffer));
    }
}

void VulkanRenderer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& out_buffer, VkDeviceMemory& out_buffer_memory)
{
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage; // Specify how the buffer is used. Can be multiple with bitwise or.
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;    // Buffers can be owned by specific queue families or shared between multiple queue families.
                                                            // This buffer will only be used by the graphics queue, so we use exclusive access.
    buffer_info.flags = 0;  // Used to configure sparse buffer memory (not relevant for us right now)

    if (vkCreateBuffer(RHI_->GetDevice()->GetLogicalDeviceHandle(), &buffer_info, nullptr, &out_buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create vertex buffer");
    }

    // Buffer was created, but no memory has been allocated yet.
    // We have to do this ourselves!

    // First query memory requirements.
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(RHI_->GetDevice()->GetLogicalDeviceHandle(), out_buffer, &mem_requirements);

    // Then allocate the memory
    // NOTE: In a real application, we shouldn't allocate memory for every single resource we create. (inefficient / max num of simultaneous mem allocations is limited)
    // Instead we should allocate a large chunk of memory and then split it up with the offset parameters by using a custom allocator.
    // See https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator for examples
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = RHI_->GetDevice()->FindMemoryType(mem_requirements.memoryTypeBits, properties);

    if (vkAllocateMemory(RHI_->GetDevice()->GetLogicalDeviceHandle(), &alloc_info, nullptr, &out_buffer_memory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }

    // Finally associate the allocated memory with the vertex buffer
    vkBindBufferMemory(RHI_->GetDevice()->GetLogicalDeviceHandle(), out_buffer, out_buffer_memory, 0 /*offset within the memory*/);
}

void VulkanRenderer::CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
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

void VulkanRenderer::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t num_mips)
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

void VulkanRenderer::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
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

bool VulkanRenderer::HasStencilComponent(VkFormat format)
{
    // We need to take the stencil component into account when performing layout transitions on images
    // -> We have to know if we actually have one or nah
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void VulkanRenderer::CreateColorResources()
{
    VkFormat color_format = viewport_->GetSwapChain()->GetSurfaceFormat().format;

    // Create multisampled color buffer
    RHI_->CreateImage(viewport_->GetWidth(), viewport_->GetHeight(), 1, num_msaa_samples_, color_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, color_image_, color_image_memory_);
    color_image_view_ = RHI_->CreateImageView(color_image_, color_format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

void VulkanRenderer::CreateDepthResources()
{
    VkFormat depth_format = RHI_->GetDevice()->FindDepthFormat();

    RHI_->CreateImage(viewport_->GetWidth(), viewport_->GetHeight(),    // should have the same resolution as the color attachment
        1,  // No mip mapping
        num_msaa_samples_,
        depth_format,    // A format that's supported by our physical device
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, // image usage appropriate for a depth attachment
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        depth_image_, depth_image_memory_
    );

    depth_image_view_ = RHI_->CreateImageView(depth_image_, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

    // Done! We don't need to map the depth image or copy another image to it, because we're going to clear it at the start of the render pass like the color attachment

    // We don't have to explicitly transition the layout of the depth image to a depth attachment because this is done in the render pass.
    // But for the sake of practicing how to do it, we'll do it now anyway :P

    TransitionImageLayout(depth_image_, depth_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
    // ^^^ undefined layout can be used as initial layout, because there are no existing depth image contents that matter.
}

void VulkanRenderer::GenerateMipmaps(VkImage image, VkFormat image_format, int32_t tex_width, int32_t tex_height, uint32_t num_mips)
{
    // Not all platforms support blitting...
    // Have to check if image format supports linear blitting first
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(RHI_->GetDevice()->GetPhysicalDeviceHandle(), image_format, &format_properties);

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

void VulkanRenderer::CreateTextureImage()
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
    VulkanBuffer staging_buffer = VulkanBuffer::Create(RHI_->GetDevice(), tex_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    memcpy(staging_buffer.Map(tex_size), tex_data, static_cast<size_t>(tex_size));
    staging_buffer.Unmap();

    stbi_image_free(tex_data); // We copied the data, so we don't need this anymore.

    // Then create the image object.
    // Theoretically we could use a buffer and bind it to the shader, but image objects are more performant and convenient
    // For example, we can use 2D coordinates to retrieve colors.
    RHI_->CreateImage(tex_width, tex_height,
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
    CopyBufferToImage(staging_buffer.GetBufferHandle(), texture_image_, static_cast<uint32_t>(tex_width), static_cast<uint32_t>(tex_height));

    // To be able to start sampling from the texture image in the shader, we need one last transition to prepare it for shader access
    //TransitionImageLayout(texture_image_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, num_mips_);

    // This final transition is already handled in GenerateMips :)
    GenerateMipmaps(texture_image_, VK_FORMAT_R8G8B8A8_SRGB, tex_width, tex_height, num_mips_);

    staging_buffer.Destroy();
}

void VulkanRenderer::CreateTextureImageView()
{
    texture_image_view_ = RHI_->CreateImageView(texture_image_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, num_mips_);
}

void VulkanRenderer::CreateTextureSampler()
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
    const VkPhysicalDeviceProperties&  properties = RHI_->GetDevice()->GetPhysicalDeviceProperties();
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
    if (vkCreateSampler(RHI_->GetDevice()->GetLogicalDeviceHandle(), &sampler_info, nullptr, &texture_sampler_) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create texture sampler!");
    }
}

VkSampleCountFlagBits VulkanRenderer::GetMaxNumSamples()
{
    const VkPhysicalDeviceProperties& physical_device_properties = RHI_->GetDevice()->GetPhysicalDeviceProperties();

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

void VulkanRenderer::LoadModel()
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
            if (unique_vertices.count(vertex) == 0)
            {
                uint32_t vertex_index = static_cast<uint32_t>(vertices_.size());
                unique_vertices[vertex] = vertex_index;
                vertices_.push_back(vertex);
            }

            indices_.push_back(unique_vertices[vertex]);
        }
    }
}

void VulkanRenderer::CreateVertexBuffer()
{
    VkDeviceSize buffer_size = sizeof(vertices_[0]) * vertices_.size();

    // Use host-visible buffer as temporary staging buffer, which is later copied to device local memory.
    // Device local memory is optimal for reading speed on the GPU, but not accessible from the CPU!
    // To copy to device local memory we therefore can't use vkMapMemory.
    // Instead we have to specify the VK_BUFFER_USAGE_TRANSFER_SRC_BIT or VK_BUFFER_USAGE_TRANSFER_DST_BIT properties.
    VulkanBuffer staging_buffer = VulkanBuffer::Create(RHI_->GetDevice(), buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    // ^^^ Properties
    // VK_BUFFER_USAGE_TRANSFER_SRC_BIT -> Buffer can be used as source in a memory transfer operation.
    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT -> We want to write to the vertex buffer from the CPU
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT -> Makes sure that data is directly written to memory
    // otherwise the writes may be cached first and are subsequently not directly available.
    // This may cost some performance
    // Alternatively we could also call vkFlushMappedMemoryRanges after writing or
    // vkInvalidateMappedMemoryRanges before reading mapped memory.

    // Map allocated memory into CPU address space, copy over vertices to staging buffer

    memcpy(staging_buffer.Map(buffer_size), vertices_.data(), (size_t)buffer_size);    // No flush required as we set VK_MEMORY_PROPERTY_HOST_COHERENT_BIT.
    staging_buffer.Unmap();

    vertex_buffer_ = VulkanBuffer::Create(RHI_->GetDevice(), buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    // ^^^
    // VK_BUFFER_USAGE_TRANSFER_DST_BIT -> Buffer can be used as destination in a memory transfer operation.

    CopyBuffer(staging_buffer.GetBufferHandle(), vertex_buffer_.GetBufferHandle(), buffer_size);

    // Once the copy command is done we can clean up the staging buffer
    staging_buffer.Destroy();
}

void VulkanRenderer::CreateIndexBuffer()
{
    // Basically same as CreateVertexBuffer, but now we create a buffer for the indices.
    // Notice the VK_BUFFER_USAGE_INDEX_BUFFER_BIT

    VkDeviceSize buffer_size = sizeof(indices_[0]) * indices_.size();

    VulkanBuffer staging_buffer = VulkanBuffer::Create(RHI_->GetDevice(), buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    memcpy(staging_buffer.Map(buffer_size), indices_.data(), (size_t)buffer_size);
    staging_buffer.Unmap();

    index_buffer_ = VulkanBuffer::Create(RHI_->GetDevice(), buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CopyBuffer(staging_buffer.GetBufferHandle(), index_buffer_.GetBufferHandle(), buffer_size);

    staging_buffer.Destroy();
}

void VulkanRenderer::CreateUniformBuffers()
{
    VkDeviceSize buffer_size = sizeof(UniformBufferObject);

    // We should not modify the uniforms of a frame that is in-flight!
    // -> We need one uniform buffer per swap chain image.
    size_t num_swapchain_images = viewport_->GetSwapChain()->GetSwapChainImages().size();
    uniform_buffers_.resize(num_swapchain_images);
    uniform_buffers_memory_.resize(num_swapchain_images);
    for (size_t i = 0; i < num_swapchain_images; i++)
    {
        // Since the uniform data is updated every frame, a staging buffer would only add unnecessary overhead.
        CreateBuffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniform_buffers_[i], uniform_buffers_memory_[i]);
    }
}

void VulkanRenderer::CreateDescriptorPool()
{
    // IMPORTANT: Inadequate descriptor pools are a good example of a problem that the validation layers will not catch
    // vkAllocateDescriptorSets may fail with the error code VK_ERROR_POOL_OUT_OF_MEMORY if the pool is not sufficiently large,
    // but the driver may also try to solve the problem internally. Sometimes we get away with exceeding the limits of the descriptor pool,
    // othertimes it fails - depending on the user's hardware.
    // This makes bugs like this hard to catch, so keep this in mind!

    size_t num_swapchain_images = viewport_->GetSwapChain()->GetSwapChainImages().size();

    std::array<VkDescriptorPoolSize, 2> pool_sizes{};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = static_cast<uint32_t>(num_swapchain_images);   // allocate one descriptor for every swap chain image
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = static_cast<uint32_t>(num_swapchain_images);

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());;
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = static_cast<uint32_t>(num_swapchain_images);

    if (vkCreateDescriptorPool(RHI_->GetDevice()->GetLogicalDeviceHandle(), &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

void VulkanRenderer::CreateDescriptorSets()
{
    size_t num_swapchain_images = viewport_->GetSwapChain()->GetSwapChainImages().size();

    std::vector<VkDescriptorSetLayout> layouts(num_swapchain_images, descriptor_set_layout_);
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = static_cast<uint32_t>(num_swapchain_images);
    alloc_info.pSetLayouts = layouts.data();

    // Create one descriptor set for each swap chain image.
    descriptor_sets_.resize(viewport_->GetSwapChain()->GetSwapChainImages().size());
    if (vkAllocateDescriptorSets(RHI_->GetDevice()->GetLogicalDeviceHandle(), &alloc_info, descriptor_sets_.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    // Then populate the descriptors inside of the descriptor sets
    for (size_t i = 0; i < viewport_->GetSwapChain()->GetSwapChainImages().size(); i++)
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

        vkUpdateDescriptorSets(RHI_->GetDevice()->GetLogicalDeviceHandle(), static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data(), 0, nullptr /*can be used to copy descriptors to each other*/);
    }
}

void VulkanRenderer::UpdateUniformData(uint32_t current_swap_chain_img_idx)
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
        viewport_->GetWidth() / static_cast<float>(viewport_->GetHeight()),  // Aspect ratio.
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
    vkMapMemory(RHI_->GetDevice()->GetLogicalDeviceHandle(), uniform_buffers_memory_[current_swap_chain_img_idx], 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(RHI_->GetDevice()->GetLogicalDeviceHandle(), uniform_buffers_memory_[current_swap_chain_img_idx]);
}

void VulkanRenderer::DrawFrame()
{
    // Wait for requested frame to be finished
    vkWaitForFences(RHI_->GetDevice()->GetLogicalDeviceHandle(), 1, &inflight_frame_fences_[current_frame_],
        VK_TRUE /*wait for all fences until return*/, UINT64_MAX /*disable time out*/);

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
    VkResult result = vkAcquireNextImageKHR(RHI_->GetDevice()->GetLogicalDeviceHandle(), viewport_->GetSwapChain()->GetHandle(),
        UINT64_MAX /*disable time out*/, image_available_semaphores_[current_frame_], VK_NULL_HANDLE, &image_index);

    // Check for window resizes, so we can recreate the swap chain.
    // VK_ERROR_OUT_OF_DATE_KHR -> Swap chain is incompatible with the surface. Typically happens on window resize, but not guaranteed.
    // VK_SUBOPTIMAL_KHR -> Some parts of the swap chain are incompatible, but we could theoretically still present to the surface.
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        CHECK_NO_ENTRY(); // TODO: Refactor
        RecreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        CHECK_NO_ENTRY();
        throw std::runtime_error("Failed to acquire swap chain image");
    }

    // If MAX_FRAMES_IN_FLIGHT is higher than the number of swap chain images or vkAcquireNextImageKHR returns images out-of-order 
    // it's possible that we may start rendering to a swap chain image that is already in flight.
    // To avoid this, we need to track for each swap chain image if a frame in flight is currently using it.
    if (inflight_images_[image_index] != VK_NULL_HANDLE)
    {
        vkWaitForFences(RHI_->GetDevice()->GetLogicalDeviceHandle(), 1, &inflight_images_[image_index], VK_TRUE, UINT64_MAX);
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
    submit_info.pCommandBuffers = &command_buffers_[image_index]->GetHandle();

    // Specify which semaphores to signal once the command buffer(s) have finished execution
    VkSemaphore signal_semaphores[] = { render_finished_semaphores_[current_frame_] };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkResetFences(RHI_->GetDevice()->GetLogicalDeviceHandle(), 1, &inflight_frame_fences_[current_frame_]);  // restore the fence to the unsignaled state 

    if (vkQueueSubmit(RHI_->GetDevice()->GetGraphicsQueue()->GetHandle(), 1,
        &submit_info, inflight_frame_fences_[current_frame_]) != VK_SUCCESS)    // Takes an array of VkSubmitInfo structs as argument for efficiency 
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
    VkSwapchainKHR swap_chains[] = { viewport_->GetSwapChain()->GetHandle() };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr;    // Optional. Allows to specify an array of VkResult values to check for every individual swap chain if presentation was successful
                                        // Not necessary if you're only using a single swap chain, because you can simply use the return value of the present function.

    // Submits the request to present an image to the swap chain
    result = vkQueuePresentKHR(RHI_->GetDevice()->GetPresentQueue()->GetHandle(), &present_info);

    // Explicitly check for window resize, so we can recreate the swap chain.
    // In this case it's important to do this after present to ensure that the semaphores are in the correct state.
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || was_frame_buffer_resized_)
    {
        CHECK_NO_ENTRY(); // TODO: Refactoring
        RecreateSwapChain();
        was_frame_buffer_resized_ = false;
    }
    else if (result != VK_SUCCESS)
    {
        CHECK_NO_ENTRY(); // TODO: Refactoring
        LOG_ERROR("Failed to present swap chain image to surface");
        exit(EXIT_FAILURE);
    }

    // Advance the frame index
    current_frame_ = (++current_frame_) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::CreateSyncObjects()
{
    image_available_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    render_finished_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    inflight_frame_fences_.resize(MAX_FRAMES_IN_FLIGHT);
    inflight_images_.resize(viewport_->GetSwapChain()->GetSwapChainImages().size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // By default we create fences in unsignaled state
                                                     // -> We'd wait indefinitely because we never submitted the fence before

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(RHI_->GetDevice()->GetLogicalDeviceHandle(), &semaphore_info, nullptr, &image_available_semaphores_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(RHI_->GetDevice()->GetLogicalDeviceHandle(), &semaphore_info, nullptr, &render_finished_semaphores_[i]) != VK_SUCCESS ||
            vkCreateFence(RHI_->GetDevice()->GetLogicalDeviceHandle(), &fence_info, nullptr, &inflight_frame_fences_[i]) != VK_SUCCESS)
        {

            throw std::runtime_error("Failed to create semaphores!");
        }
    }
}
