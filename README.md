# VulkanSandbox

A Vulkan sandbox application for educational purposes.
This project demonstrates the fundamentals to initialize Vulkan and render a simple model.
The code is written in C++ and heavily inspired by the [Vulkan Tutorial](https://vulkan-tutorial.com/).

## How to build (Win64)

1. Install `Vulkan` if you haven't already.
2. Make sure the `VULKAN_SDK` environment variable is set up properly.
3. Run `GenerateProjectFiles.bat`.
4. Open the generated `VulkanSandbox.sln` with VS2019.
5. Build and run in the desired configuration (debug / release)

## Dependencies

* [STB](https://github.com/nothings/stb)
* [GLFW](https://www.glfw.org/)
* [GLM](https://glm.g-truc.net/)
* [Vulkan](https://vulkan.lunarg.com/)

## Acknowledgments

* [Vulkan Tutorial](https://vulkan-tutorial.com/)