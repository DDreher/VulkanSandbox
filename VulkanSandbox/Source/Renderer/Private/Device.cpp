#include "Device.h"

Device::Device(VkPhysicalDevice physical_device)
    : physical_device_(physical_device)
{
    assert(physical_device != VK_NULL_HANDLE);

    // Query info for later use
    vkGetPhysicalDeviceProperties(physical_device_, &physical_device_properties);
    vkGetPhysicalDeviceFeatures(physical_device_, &physical_device_features);

    uint32_t num_extension_props = 0;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &num_extension_props, nullptr);
    std::vector<VkExtensionProperties> extensions(num_extension_props);
    if (num_extension_props > 0 && vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &num_extension_props, &extensions.front()) == VK_SUCCESS)
    {
        for (const auto& extension_prop : extensions)
        {
            supported_extensions_.push_back(extension_prop.extensionName);
        }
    }
}

Device::~Device()
{
    if(logical_device_ != VK_NULL_HANDLE)
    {
        vkDestroyDevice(logical_device_, nullptr);
    }
}

VkResult Device::CreateLogicalDevice(const DeviceCreationProperties& device_creation_props)
{
    FindQueueFamilies(device_creation_props.surface);

    std::set<uint32_t> unique_queue_family_indices = { queue_family_indices_.compute_family.value(), queue_family_indices_.transfer_family.value() };
    if(device_creation_props.is_headless == false)
    {
        assert(queue_family_indices_.graphics_family.has_value());
        assert(queue_family_indices_.present_family.has_value());
        unique_queue_family_indices.insert(queue_family_indices_.graphics_family.value());
        unique_queue_family_indices.insert(queue_family_indices_.present_family.value());
    }

    // Queue creation infos for all required families
    float queue_priority = 1.0f;    // Queue priorities [0.0f, 1.0f] influence the scheduling of command buffer execution.
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    for (uint32_t queue_family_index : unique_queue_family_indices)
    {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family_index;
        queue_create_info.queueCount = 1;   // We only need one queue, because we can create command buffers on multiple threads and submit them all at once.
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    // Check extension support.
    if(AreExtensionsSupported(device_creation_props.extensions))
    {
        LOG("Failed to create logical device - Not all required extension not supported.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_creation_props.features;
    create_info.enabledExtensionCount = static_cast<uint32_t>(device_creation_props.extensions.size());
    create_info.ppEnabledExtensionNames = device_creation_props.extensions.data();
    create_info.enabledLayerCount = static_cast<uint32_t>(device_creation_props.valiation_layers.size());
    create_info.ppEnabledLayerNames = device_creation_props.valiation_layers.data();

    // TODO: Get an dsave the queues

    return vkCreateDevice(physical_device_, &create_info, nullptr, &logical_device_);
}

bool Device::AreExtensionsSupported(const std::vector<const char*>& extensions) const
{
    assert(supported_extensions_.size() > 0); // We definitely should have acquired supported extensions by the time this is called

    std::set<std::string> required_extensions(extensions.begin(), extensions.end());
    for (const auto& extension : supported_extensions_)
    {
        required_extensions.erase(extension);
    }

    return required_extensions.empty();
}

QueueFamilyIndices Device::FindQueueFamilies(VkSurfaceKHR surface) const
{
    assert(physical_device_ != nullptr);

    QueueFamilyIndices indices;

    // VkQueueFamilyProperties contains details about the queue family,
    // e.g. the type of operations that are supported and the number of queues that can be created based on that family
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, queue_families.data());

    // Look for a graphics and present queue family
    for (uint32_t i=0; i<queue_family_count; ++i)
    {
        const VkQueueFamilyProperties& queue_family = queue_families[i];
        bool is_graphics_queue_family = queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT;
        if (is_graphics_queue_family)
        {
            indices.graphics_family = i;
        }

        // the graphics queue family does not necessarily also support presenting to a surface
        // -> We have to add an additional check and remember the queue family that supports it.
        // This COULD be the same queue family as the graphics family, though.
        // To maximize performance we could even try to find a family that is required to support both graphics and presenting here.
        VkBool32 is_present_supported = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_, i, surface, &is_present_supported);
        if (is_present_supported)
        {
            indices.present_family = i;
        }

        if (is_graphics_queue_family && is_present_supported)
        {
            // That would be perfect :)
            break;
        }
    }

    // Compute queue family
    for (uint32_t i = 0; i < queue_family_count; ++i)
    {
        const VkQueueFamilyProperties& queue_family = queue_families[i];
        bool is_compute_queue_family = queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT;
        if (is_compute_queue_family)
        {
            indices.compute_family = i;

            // Prefer a dedicated queue family
            if(indices.graphics_family != indices.compute_family)
            {
                break;
            }
        }
    }

    // Transfer queue family
    for (uint32_t i = 0; i < queue_family_count; ++i)
    {
        const VkQueueFamilyProperties& queue_family = queue_families[i];
        bool is_transfer_queue_family = queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT;
        if (is_transfer_queue_family)
        {
            indices.transfer_family = i;

            // Prefer a dedicated queue family
            if (indices.graphics_family != indices.transfer_family)
            {
                break;
            }
        }
    }

    return indices;
}
