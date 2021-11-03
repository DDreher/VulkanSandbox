#pragma once
#include "VulkanGenericPlatform.h"

class VulkanWindowsPlatform : public VulkanGenericPlatform
{
public:
    static void GetInstanceExtensions(std::vector<const char*>& out_extensions);
    static void GetDeviceExtensions(std::vector<const char*>& out_extensions);
};

typedef VulkanWindowsPlatform VulkanPlatform;
