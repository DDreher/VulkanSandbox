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
    int32 graphics_queue_family_idx = -1;
    int32 compute_queue_family_idx = -1;
    int32 transfer_queue_family_idx = -1;

    for(int32 i = 0; i < queue_family_properties_.size(); ++i)
    {
        bool is_queue_family_used = false;
        const VkQueueFamilyProperties& properties = queue_family_properties_[i];

        // Note: the graphics queue family does not necessarily support presenting to a surface...
        // to keep the code simple we neglect this for now.
        if(properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            if(graphics_queue_family_idx == -1)
            {
                graphics_queue_family_idx = i;
                is_queue_family_used = true;
            }
        }

        if (properties.queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            // Prefer non-graphics compute queue
            if (compute_queue_family_idx == -1 && graphics_queue_family_idx != compute_queue_family_idx)
            {
                compute_queue_family_idx = i;
                is_queue_family_used = true;
            }
        }

        if (properties.queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            // Prefer dedicated transfer queue
            if (transfer_queue_family_idx == -1 && !(properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) && !(properties.queueFlags & VK_QUEUE_COMPUTE_BIT))
            {
                transfer_queue_family_idx = i;
                is_queue_family_used = true;
            }
        }

        if(is_queue_family_used)
        {
            // TODO: Is this correct?
            float queue_priority = 1.0f;    // Queue priorities [0.0f, 1.0f] influence the scheduling of command buffer execution.
            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = i;
            queue_create_info.queueCount = 1;   // We only need one queue, because we can create command buffers on multiple threads and submit them all at once.
            queue_create_info.pQueuePriorities = &queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }
    }

    device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    device_create_info.pQueueCreateInfos = queue_create_infos.data();

    VkResult result = vkCreateDevice(physical_device_, &device_create_info, nullptr, &logical_device_);
    VERIFY_VK_RESULT(result);
    if(result == VK_ERROR_INITIALIZATION_FAILED)
    {
        LOG_ERROR("Failed to initialize logical device.");
        exit(EXIT_FAILURE);
    }

    LOG("Using device layers:");
    for (const char* layer : device_layers_)
    {
        LOG("- {}", layer);
    }

    LOG("Using device extensions:");
    for (const char* extension : device_extensions_)
    {
        LOG("- {}", extension);
    }

    // Wrap the Vulkan queues
    graphics_queue_ = new VulkanQueue(this, graphics_queue_family_idx);
    
    bool found_dedicated_compute_queue = compute_queue_family_idx != -1;
    if(!found_dedicated_compute_queue)
    {
        // Use default queue
        compute_queue_family_idx = graphics_queue_family_idx;
    }
    compute_queue_ = new VulkanQueue(this, compute_queue_family_idx);

    bool found_dedicated_transfer_queue = transfer_queue_family_idx != -1;
    if(!found_dedicated_transfer_queue)
    {
        // Use default queue
        transfer_queue_family_idx = compute_queue_family_idx;
    }
    transfer_queue_ = new VulkanQueue(this, transfer_queue_family_idx);
}

bool VulkanDevice::AreExtensionsSupported(const std::vector<const char*>& extensions_to_check) const
{
    CHECK(device_extensions_.size() > 0); // We definitely should have acquired supported extensions by the time this is called
    std::set<std::string> extensions(extensions_to_check.begin(), extensions_to_check.end());
    for (const auto& extension : device_extensions_)
    {
        extensions.erase(extension);
    }

    return extensions.empty();
}

void VulkanDevice::WaitUntilIdle()
{
    CHECK(logical_device_ != VK_NULL_HANDLE);
    vkDeviceWaitIdle(logical_device_);
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
    required_layers.push_back(KHRONOS_VALIDATION_LAYER_NAME));
#endif // _RENDER_DEBUG

    return required_layers;
}
