#include "VulkanDevice.h"

#include "VulkanDefines.h"
#include "VulkanMacros.h"
#include "VulkanPlatform.h"
#include "VulkanQueue.h"
#include "VulkanRHI.h"
#include "VulkanUtils.h"

VulkanDevice::VulkanDevice(VulkanRHI* RHI, VkPhysicalDevice physical_device, int32 device_idx)
    : RHI_(RHI),
    physical_device_(physical_device),
    device_idx_(device_idx)
{
}

VulkanDevice::~VulkanDevice()
{
}

void VulkanDevice::Destroy()
{
    WaitUntilIdle();

    delete graphics_queue_;
    graphics_queue_ = nullptr;
    delete transfer_queue_;
    transfer_queue_ = nullptr;
    delete compute_queue_;
    compute_queue_ = nullptr;
    delete present_queue_;
    present_queue_ = nullptr;

    CHECK(logical_device_ != VK_NULL_HANDLE);
    vkDestroyDevice(logical_device_, nullptr);
    logical_device_ = VK_NULL_HANDLE;
}

void VulkanDevice::QueryGPUInfo()
{
    QuerySupportedDeviceExtensions();
    QuerySupportedDeviceValidationLayers();
    vkGetPhysicalDeviceProperties(physical_device_, &physical_device_properties_);

    std::string device_type;
    switch(physical_device_properties_.deviceType)
    {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        device_type = "Discrete GPU";
        is_discrete_ = true;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        device_type = "Integrated GPU";
        break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        device_type = "Virtual GPU";
        break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        device_type = "CPU";
        break;
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        device_type = "Other";
        break;
    default:
        device_type = "Unknown";
        break;
    }

    LOG("Device {}: {}", device_idx_, physical_device_properties_.deviceName);
    LOG("- Type: {}", device_type);
    LOG("- API: {}.{}.{}",
        VK_VERSION_MAJOR(physical_device_properties_.apiVersion),
        VK_VERSION_MINOR(physical_device_properties_.apiVersion),
        VK_VERSION_PATCH(physical_device_properties_.apiVersion));

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);
    CHECK(queue_family_count > 0);
    queue_family_properties_.resize(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, queue_family_properties_.data());
}

void VulkanDevice::QuerySupportedDeviceExtensions()
{
    uint32 count = 0;
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &count, nullptr);
    supported_extensions_.resize(count);
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &count, supported_extensions_.data());
}

void VulkanDevice::QuerySupportedDeviceValidationLayers()
{
    uint32 count = 0;
    vkEnumerateDeviceLayerProperties(physical_device_, &count, nullptr);
    supported_validation_layers_.resize(count);
    vkEnumerateDeviceLayerProperties(physical_device_, &count, supported_validation_layers_.data());
}

void VulkanDevice::CreateLogicalDevice()
{
    CHECK(physical_device_ != VK_NULL_HANDLE);
    CHECK(logical_device_ == VK_NULL_HANDLE);

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    // Setup device features
    vkGetPhysicalDeviceFeatures(physical_device_, &physical_device_features_);
    // Disable sparse features
    physical_device_features_.shaderResourceResidency = VK_FALSE;
    physical_device_features_.shaderResourceMinLod = VK_FALSE;
    physical_device_features_.sparseBinding = VK_FALSE;
    physical_device_features_.sparseResidencyBuffer = VK_FALSE;
    physical_device_features_.sparseResidencyImage2D = VK_FALSE;
    physical_device_features_.sparseResidencyImage3D = VK_FALSE;
    physical_device_features_.sparseResidency2Samples = VK_FALSE;
    physical_device_features_.sparseResidency4Samples = VK_FALSE;
    physical_device_features_.sparseResidency8Samples = VK_FALSE;
    physical_device_features_.sparseResidencyAliased = VK_FALSE;
    device_create_info.pEnabledFeatures = &physical_device_features_;

    // Set up extensions 
    std::vector<const char*> required_extensions = GetRequiredExtensions();
    std::vector<const char*> supported_extensions_char_ptrs;
    for (const VkExtensionProperties& ext_prop : supported_extensions_)
    {
        supported_extensions_char_ptrs.push_back(ext_prop.extensionName);
    }

    bool are_required_extensions_supported = VulkanUtils::IsListSubset(supported_extensions_char_ptrs, required_extensions);
    if (are_required_extensions_supported == false)
    {
        LOG_ERROR("Failed to create Vulkan device: Not all required extensions supported!");
        exit(EXIT_FAILURE);
    }
    device_create_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
    device_create_info.ppEnabledExtensionNames = required_extensions.data();

    // Set up validation layers
    std::vector<const char*> required_layers = GetRequiredValidationLayers();
    std::vector<const char*> supported_layers_char_ptrs;
    for(const VkLayerProperties& layer_prop : supported_validation_layers_)
    {
        supported_layers_char_ptrs.push_back(layer_prop.layerName);
    }

    bool are_required_layers_supported = VulkanUtils::IsListSubset(supported_layers_char_ptrs, required_layers);
    if (are_required_layers_supported == false)
    {
        LOG_ERROR("Failed to create Vulkan device: Not all required validation layers supported!");
        exit(EXIT_FAILURE);
    }
    device_create_info.enabledLayerCount = static_cast<uint32_t>(required_layers.size());
    device_create_info.ppEnabledLayerNames = required_layers.data();

    // Set up queues
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    uint32_t requested_queue_families = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
    QueueFamilyIndices queue_family_indices = GetQueueFamilyIndices(requested_queue_families);

    static const float default_queue_priority = 0.0f;    // Queue priorities [0.0f, 1.0f] influence the scheduling of command buffer execution.

    if(requested_queue_families & VK_QUEUE_GRAPHICS_BIT)
    {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family_indices.graphics;
        queue_create_info.queueCount = 1;   // We only need one queue, because we can create command buffers on multiple threads and submit them all at once.
        queue_create_info.pQueuePriorities = &default_queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    if (requested_queue_families & VK_QUEUE_COMPUTE_BIT)
    {
        if(queue_family_indices.compute != queue_family_indices.graphics)
        {
            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_family_indices.compute;
            queue_create_info.queueCount = 1;   // We only need one queue, because we can create command buffers on multiple threads and submit them all at once.
            queue_create_info.pQueuePriorities = &default_queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }
    }

    if (requested_queue_families & VK_QUEUE_TRANSFER_BIT)
    {
        if (queue_family_indices.transfer != queue_family_indices.graphics && queue_family_indices.transfer != queue_family_indices.compute)
        {
            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_family_indices.transfer;
            queue_create_info.queueCount = 1;   // We only need one queue, because we can create command buffers on multiple threads and submit them all at once.
            queue_create_info.pQueuePriorities = &default_queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }
    }

    device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    device_create_info.pQueueCreateInfos = queue_create_infos.data();

    VkResult result = vkCreateDevice(physical_device_, &device_create_info, nullptr, &logical_device_);
    VERIFY_VK_RESULT(result);

    // Wrap the Vulkan queues
    graphics_queue_ = new VulkanQueue(this, queue_family_indices.graphics);
    compute_queue_ = new VulkanQueue(this, queue_family_indices.compute);
    transfer_queue_ = new VulkanQueue(this, queue_family_indices.transfer);

    if(result == VK_ERROR_INITIALIZATION_FAILED)
    {
        LOG_ERROR("Failed to initialize logical device.");
        exit(EXIT_FAILURE);
    }

    LOG("Using device layers:");
    for (const char* layer : required_layers)
    {
        LOG("- {}", layer);
    }

    LOG("Using device extensions:");
    for (const char* extension : required_extensions)
    {
        LOG("- {}", extension);
    }
}

void VulkanDevice::WaitUntilIdle()
{
    CHECK(logical_device_ != VK_NULL_HANDLE);
    vkDeviceWaitIdle(logical_device_);
}

void VulkanDevice::InitPresentQueue(VkSurfaceKHR surface)
{
    CHECK(present_queue_ == nullptr);
    CHECK(physical_device_ != VK_NULL_HANDLE);

    VkBool32 is_present_supported;

    // For now we simply check the already existing queues for present support
    VERIFY_VK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_, graphics_queue_->GetFamilyIndex(), surface, &is_present_supported));
    if(is_present_supported)
    {
        present_queue_ = graphics_queue_;
        LOG("Using graphics queue as present queue");
        return;
    }

    VERIFY_VK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_, compute_queue_->GetFamilyIndex(), surface, &is_present_supported));
    if (is_present_supported)
    {
        present_queue_ = compute_queue_;
        LOG("Using graphics queue as present queue");
        return;
    }

    VERIFY_VK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_, transfer_queue_->GetFamilyIndex(), surface, &is_present_supported));
    if (is_present_supported)
    {
        present_queue_ = transfer_queue_;
        LOG("Using transfer queue as present queue");
        return;
    }

    LOG_ERROR("Could not find a present queue");
    exit(EXIT_FAILURE);
}

VkFormat VulkanDevice::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
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

VkFormat VulkanDevice::FindDepthFormat()
{
    // We have to specify the accuracy of our depth image:
    // VK_FORMAT_D32_SFLOAT: 32 - bit float for depth
    // VK_FORMAT_D32_SFLOAT_S8_UINT : 32 - bit signed float for depth and 8 bit stencil component
    // VK_FORMAT_D24_UNORM_S8_UINT : 24 - bit float for depth and 8 bit stencil component

    return FindSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

std::vector<const char*> VulkanDevice::GetRequiredExtensions() const
{
    std::vector<const char*> required_extensions;
    VulkanPlatform::GetDeviceExtensions(required_extensions);
    required_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    return required_extensions;
}

std::vector<const char*> VulkanDevice::GetRequiredValidationLayers() const
{
    std::vector<const char*> required_layers;

#ifdef _RENDER_DEBUG
    // If debugging, we add the standard khronos validation layers
    required_layers.push_back(KHRONOS_VALIDATION_LAYER_NAME);
#endif // _RENDER_DEBUG

    return required_layers;
}

QueueFamilyIndices VulkanDevice::GetQueueFamilyIndices(uint32_t requested_family_flags) const
{
    QueueFamilyIndices indices;

    // Find dedicated compute queue family
    if (requested_family_flags & VK_QUEUE_COMPUTE_BIT)
    {
        for (int32 i = 0; i < queue_family_properties_.size(); ++i)
        {
            const VkQueueFamilyProperties& properties = queue_family_properties_[i];
            if ((properties.queueFlags & VK_QUEUE_COMPUTE_BIT) && (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
            {
                indices.compute = i;
            }
        }
    }

    // Find dedicated transfer queue family
    if (requested_family_flags & VK_QUEUE_TRANSFER_BIT)
    {
        for (int32 i = 0; i < queue_family_properties_.size(); ++i)
        {
            const VkQueueFamilyProperties& properties = queue_family_properties_[i];
            if ((properties.queueFlags & VK_QUEUE_TRANSFER_BIT) && (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0
                &&(properties.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)
            {
                indices.transfer = i;
            }
        }
    }

    // For other queue types or if we can't find dedicated queues, we'll just use whatever we can get our hands on
    for (int32 i = 0; i < queue_family_properties_.size(); ++i)
    {
        const VkQueueFamilyProperties& properties = queue_family_properties_[i];

        if(requested_family_flags & VK_QUEUE_TRANSFER_BIT && indices.transfer == -1 &&
            properties.queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            indices.transfer = i;
        }

        if (requested_family_flags & VK_QUEUE_COMPUTE_BIT && indices.compute == -1 &&
            properties.queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            indices.compute = i;
        }

        if (requested_family_flags & VK_QUEUE_GRAPHICS_BIT && indices.graphics == -1 &&
            properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphics = i;
        }
    }

    return indices;
}
