#include "VulkanRenderPass.h"

#include "VulkanContext.h"

VulkanRenderPass::VulkanRenderPass(VulkanSwapchain* swapchain)
{
    CHECK(VulkanContext::Get().IsInitialized());
    CHECK(swapchain != nullptr);

    VulkanContext& vulkan_context = VulkanContext::Get();

    // TODO: We have to be able to define all of this from the outside...

    VkSampleCountFlagBits num_msaa_samples = VK_SAMPLE_COUNT_1_BIT;

    VkAttachmentDescription color_attachment{};
    color_attachment.format = swapchain->GetSurfaceFormat().format; // should match the format of the swap chain images
    color_attachment.samples = num_msaa_samples;   // No multisampling for now -> Only 1 sample.
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
    color_attachment_resolve.format = swapchain->GetSurfaceFormat().format;
    color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = vulkan_context.GetDevice()->FindDepthFormat();
    depth_attachment.samples = num_msaa_samples;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;    // depth data will not be used after drawing has finished (may allow hardware optimizations)
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;     // we don't care about the previous depth contents
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Subpasses and attachment references
    // Render pass consists  of (multiple) subpasses.
    // Subpasses -> subsequent rendering operations that depend on frame buffers of previous passes (e.g. post-processing effects).

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;    // specifies which attachment to reference by its index in the attachment descriptions array of VkRenderPassCreateInfo
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // layout we would like attachment to have during subpass
                                                                            // -> Automatic transition to this layout on subpass begin

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

    //if (num_msaa_samples != VK_SAMPLE_COUNT_1_BIT)
    //{
        subpass.pResolveAttachments = &color_attachment_resolve_ref;    // This is enough to let the render pass define a multisample resolve operation
                                                                        // which will let us render the image to screen
    //}

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

    VkResult result = vkCreateRenderPass(vulkan_context.GetDevice()->GetLogicalDeviceHandle(), &render_pass_info, nullptr, &render_pass_);
    if (result != VK_SUCCESS)
    {
        LOG_ERROR("Failed to create VulkanRenderPass");
        exit(EXIT_FAILURE);
    }
}

VulkanRenderPass::~VulkanRenderPass()
{
    vkDestroyRenderPass(VulkanContext::Get().GetDevice()->GetLogicalDeviceHandle(), render_pass_, nullptr);
    render_pass_ = VK_NULL_HANDLE;
}

