#pragma once
#include "VulkanPlatformGeneric.h"

class VulkanPlatformWindows : public VulkanPlatformGeneric
{
public:
    static void GetInstanceExtensions(std::vector<const char*>& out_extensions);
    static void GetDeviceExtensions(std::vector<const char*>& out_extensions);
};

typedef VulkanPlatformWindows VulkanPlatform;
