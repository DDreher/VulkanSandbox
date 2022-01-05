#pragma once

#define GLFW_INCLUDE_VULKAN // GLFW will load vulkan.h
#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "TickTimer.h"
#include "VulkanContext.h"

class VulkanApplication
{
public:
    void Run();

private:
    void MainLoop();
    void Cleanup();

    void InitWindow();
    void DestroyWindow();
    static void WindowResizeCallback(GLFWwindow* window, int width, int height);

    void Update(float delta);
    void Render();

    bool is_running_ = false;

    GLFWwindow* window_ = nullptr;
    const std::string WINDOW_TITLE_ = std::string("Vulkan Sandbox");
    const uint32_t INITIAL_SCREEN_WIDTH_ = 800;
    const uint32_t INITIAL_SCREEN_HEIGHT_ = 600;

    TickTimer tick_timer_;

    VulkanContext RHI_;
    VulkanRenderer renderer_;
};
