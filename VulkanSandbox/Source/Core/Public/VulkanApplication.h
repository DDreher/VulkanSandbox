#pragma once

#define GLFW_INCLUDE_VULKAN // GLFW will load vulkan.h
#include <GLFW/glfw3.h>

#include "Renderer.h"

class VulkanApplication
{
public:
    void Run();

private:
    void InitWindow();
    static void WindowResizeCallback(GLFWwindow* window, int width, int height);

    void MainLoop();

    void Cleanup();

    VulkanRenderer Renderer_;
    GLFWwindow* window_ = nullptr;
    const std::string WINDOW_TITLE_ = std::string("Vulkan Sandbox");
    const uint32_t SCREEN_WIDTH_ = 800;
    const uint32_t SCREEN_HEIGHT_ = 600;
};
