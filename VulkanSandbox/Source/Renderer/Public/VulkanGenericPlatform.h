#pragma once

class VulkanGenericPlatform
{
public:
    static void GetInstanceExtensions(std::vector<const char*>& out_extensions);
    static void GetDeviceExtensions(std::vector<const char*>& out_extensions);
};
