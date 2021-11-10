#include "VulkanRHI.h"

#include "vulkan/vulkan_core.h"

#include "VulkanMacros.h"

VulkanRHI::VulkanRHI()
{
}

void VulkanRHI::Init()
{
    instance_.Init();
    SelectAndInitDevice();
}

void VulkanRHI::Shutdown()
{
    for(VulkanDevice* device : found_devices_)
    {
        CHECK(device != nullptr);
        device->Destroy();
        delete device;
        device = nullptr;
    }
    found_devices_.clear();
    device_ = nullptr;

    instance_.Shutdown();
}

void VulkanRHI::SelectAndInitDevice()
{
    uint32 gpu_count = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance_.GetHandle(), &gpu_count, nullptr);
    CHECK_MSG(gpu_count > 0, "Failed to find GPU / driver with Vulkan support!");

    std::vector<VkPhysicalDevice> physical_devices(gpu_count);
    VERIFY_VK_RESULT(vkEnumeratePhysicalDevices(instance_.GetHandle(), &gpu_count, physical_devices.data()));
    LOG("Found {} GPU(s)", gpu_count);

    // Create device objects and find the GPU that fits our needs best
    for(uint32 i = 0; i<gpu_count; ++i)
    {
        const VkPhysicalDevice& device_handle = physical_devices[i];
        VulkanDevice* device = new VulkanDevice(this, device_handle, i);
        found_devices_.push_back(device);
        device->QueryGPUInfo();

        if(device->IsDiscrete())
        {
            // For now we'll just use the first discrete GPU we find.
            // We could also add more complicated logic here (check GPU vendors, etc) but for this small application that would be overkill...
            device_ = device;
            break;
        }
    }

    if(device_ == nullptr)
    {
        // As a last resort we'll just use the first GPU we found.
        LOG_WARN("Could not find discrete GPU! Using any other GPU instead.");
        device_ = found_devices_[0];
    }

    if(device_ == nullptr)
    {
        LOG_ERROR("Failed to find a suitable device!");
        exit(EXIT_FAILURE);
    }

    LOG("Using device: {}", device_->GetDeviceIndex());
    device_->CreateLogicalDevice();
}
