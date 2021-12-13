#include "VulkanPlatformWindows.h"

#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_win32.h"

void VulkanPlatformWindows::GetInstanceExtensions(std::vector<const char*>& out_extensions)
{
    out_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    out_extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
}

void VulkanPlatformWindows::GetDeviceExtensions(std::vector<const char*>& out_extensions)
{

}
