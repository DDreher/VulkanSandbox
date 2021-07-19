# VulkanSandbox

A Vulkan sandbox application for educational purposes, mostly based on https://vulkan-tutorial.com/.

## How to build

Building has only been tested on Win x64.

1. Make sure to have Vulkan installed
2. Make sure the `VULKAN_SDK` environment variable is set up properly
3. Run `GenerateProjectFiles.bat`
4. Open the generated `VulkanSandbox.sln`
5. Build and run in the desired configuration (debug / release)

## Dependencies

* Vulkan
* GLFW
* GLM