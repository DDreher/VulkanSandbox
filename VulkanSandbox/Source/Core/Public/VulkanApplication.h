#pragma once

#define GLFW_INCLUDE_VULKAN // GLFW will load vulkan.h
#include <GLFW/glfw3.h>

#include "KeyboardEventHandler.h"
#include "Renderer.h"
#include "TickTimer.h"
#include "Window.h"
#include "WindowEventHandler.h"

class VulkanApplication
{
public:
    void Run();

private:
    void InitWindow();
    void MainLoop();
    void Cleanup();

    static void WindowResizeCallback(GLFWwindow* window, int width, int height);

    void Update(float delta);
    void Render();

    bool is_running_ = false;

    GLFWwindow* window_ = nullptr;
    const std::string WINDOW_TITLE_ = std::string("Vulkan Sandbox");
    const uint32_t INITIAL_SCREEN_WIDTH_ = 800;
    const uint32_t INITIAL_SCREEN_HEIGHT_ = 600;

    TickTimer tick_timer_;
    VulkanRenderer Renderer_;
};
