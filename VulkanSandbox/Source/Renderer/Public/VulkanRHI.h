#pragma once
#include "vulkan/vulkan_core.h"
#include "VulkanInstance.h"

class VulkanDevice;

class VulkanRHI
{
public:
    VulkanRHI();
    ~VulkanRHI() {};

    /**
    *    Initializes the RHI using Vulkan as backend.
    *    Creates the Vulkan Instance and selects the physical and logical devices to use.
    */
    void Init();

    //void PostInit();

    /**
    *    Cleans up the RHI.
    *    Destroys devices.
    */
    void Shutdown();

    const std::vector<char*>& GetInstanceExtensions() const
    {
        return instance_extensions_;
    }

    const std::vector<char*>& GetInstanceLayers() const
    {
        return instance_layers_;
    }

    //static void DestroySwapChain();
    //static void RecreateSwapChain(void* NewNativeWindow);

    const VulkanInstance& GetInstance() const
    {
        return instance_;
    }

    VulkanDevice* GetDevice()
    {
        return device_;
    }

private:
    void SelectAndInitDevice();

    VulkanInstance instance_;
    
    std::vector<char*> instance_extensions_;
    std::vector<char*> instance_layers_;

    std::vector<VulkanDevice*> found_devices_;
    VulkanDevice* device_ = nullptr;
};
